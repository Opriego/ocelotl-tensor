// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/codegen/llvm/TargetCodeGenerator.hpp"

#include <gtest/gtest.h>

#include <llvm/ADT/SmallString.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Object/Binary.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/FileSystem.h>
#if LLVM_VERSION_MAJOR >= 16
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#else
#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>
#endif

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace ocelotl::codegen;

namespace {

std::unique_ptr<llvm::Module> makeValidModule(llvm::LLVMContext &context) {
  auto module = std::make_unique<llvm::Module>("target_test", context);
  auto *type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false);
  auto *function = llvm::Function::Create(type, llvm::Function::ExternalLinkage,
                                          "main", module.get());
  auto *entry = llvm::BasicBlock::Create(context, "entry", function);
  llvm::IRBuilder<> builder{entry};
  builder.CreateRet(builder.getInt32(0));
  return module;
}

std::filesystem::path temporaryPath(const llvm::StringRef suffix) {
  llvm::SmallString<128> path;
  const std::error_code error =
      llvm::sys::fs::createTemporaryFile("ocelotl-target-test", suffix, path);
  if (error) {
    throw std::runtime_error{"could not create test output: " +
                             error.message()};
  }
  return std::filesystem::path{path.str().str()};
}

} // namespace

TEST(TargetCodeGeneratorTest, ConfiguresHostTripleAndDataLayout) {
  llvm::LLVMContext context;
  auto module = makeValidModule(context);
  TargetCodeGenerator generator;

  generator.configureModule(*module);

  EXPECT_EQ(module->getTargetTriple(), llvm::sys::getDefaultTargetTriple());
  EXPECT_FALSE(module->getDataLayout().isDefault());
}

TEST(TargetCodeGeneratorTest, EmitsHostAssembly) {
  llvm::LLVMContext context;
  auto module = makeValidModule(context);
  TargetCodeGenerator generator;
  const auto path = temporaryPath("s");

  ASSERT_NO_THROW(
      generator.emit(*module, NativeOutputKind::Assembly, path.string()));
  EXPECT_GT(std::filesystem::file_size(path), 0U);
  std::filesystem::remove(path);
}

TEST(TargetCodeGeneratorTest, EmitsRelocatableObjectForHostArchitecture) {
  llvm::LLVMContext context;
  auto module = makeValidModule(context);
  TargetCodeGenerator generator;
  const auto path = temporaryPath("o");

  ASSERT_NO_THROW(
      generator.emit(*module, NativeOutputKind::Object, path.string()));

  auto binaryOrError = llvm::object::createBinary(path.string());
  ASSERT_TRUE(static_cast<bool>(binaryOrError));
  const auto *object =
      llvm::dyn_cast<llvm::object::ObjectFile>(binaryOrError->getBinary());
  ASSERT_NE(object, nullptr);

  const llvm::Triple hostTriple{llvm::sys::getDefaultTargetTriple()};
  EXPECT_EQ(object->getArch(), hostTriple.getArch());
  EXPECT_TRUE(object->isRelocatableObject());
  std::filesystem::remove(path);
}

TEST(TargetCodeGeneratorTest, RejectsInvalidTargetTriple) {
  EXPECT_THROW(
      TargetCodeGenerator(TargetConfiguration{"not-a-real-target", "", ""}),
      std::runtime_error);
}

TEST(TargetCodeGeneratorTest, ReportsModuleVerificationFailure) {
  llvm::LLVMContext context;
  llvm::Module module{"invalid", context};
  auto *type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
  auto *function = llvm::Function::Create(type, llvm::Function::ExternalLinkage,
                                          "broken", module);
  llvm::BasicBlock::Create(context, "entry", function);
  TargetCodeGenerator generator;

  try {
    generator.verifyModule(module);
    FAIL() << "Expected verification to fail";
  } catch (const std::runtime_error &error) {
    EXPECT_NE(std::string{error.what()}.find("invalid LLVM module"),
              std::string::npos);
  }
}

TEST(TargetCodeGeneratorTest, NormalizesExplicitTargetTriple) {
  TargetCodeGenerator generator{
      TargetConfiguration{"aarch64-linux-gnu", "generic", ""}};
  EXPECT_EQ(generator.configuration().triple,
            "aarch64-unknown-linux-gnu");
}

TEST(TargetCodeGeneratorTest, EmitsExplicitX86LinuxELFObject) {
  llvm::LLVMContext context;
  auto module = makeValidModule(context);
  TargetCodeGenerator generator{
      TargetConfiguration{"x86_64-unknown-linux-gnu", "generic", ""}};
  const auto path = temporaryPath("x86.o");

  ASSERT_NO_THROW(
      generator.emit(*module, NativeOutputKind::Object, path.string()));
  auto binaryOrError = llvm::object::createBinary(path.string());
  ASSERT_TRUE(static_cast<bool>(binaryOrError));
  const auto* object = llvm::dyn_cast<llvm::object::ObjectFile>(
      binaryOrError->getBinary());
  ASSERT_NE(object, nullptr);
  EXPECT_TRUE(object->isELF());
  EXPECT_TRUE(object->isRelocatableObject());
  EXPECT_EQ(object->getArch(), llvm::Triple::x86_64);
  std::filesystem::remove(path);
}

TEST(TargetCodeGeneratorTest, EmitsAArch64LinuxAssemblyAndELFObject) {
  llvm::LLVMContext context;
  auto assemblyModule = makeValidModule(context);
  auto objectModule = makeValidModule(context);
  std::unique_ptr<TargetCodeGenerator> generator;
  try {
    generator = std::make_unique<TargetCodeGenerator>(
        TargetConfiguration{"aarch64-unknown-linux-gnu", "generic", ""});
  } catch (const std::runtime_error& error) {
    GTEST_SKIP() << "installed LLVM has no AArch64 backend: " << error.what();
  }

  const auto assemblyPath = temporaryPath("aarch64.s");
  const auto objectPath = temporaryPath("aarch64.o");
  ASSERT_NO_THROW(generator->emit(
      *assemblyModule, NativeOutputKind::Assembly, assemblyPath.string()));
  ASSERT_NO_THROW(generator->emit(
      *objectModule, NativeOutputKind::Object, objectPath.string()));

  EXPECT_GT(std::filesystem::file_size(assemblyPath), 0U);
  std::ifstream assembly{assemblyPath};
  const std::string assemblyText{
      std::istreambuf_iterator<char>{assembly},
      std::istreambuf_iterator<char>{}};
  EXPECT_NE(assemblyText.find("ret"), std::string::npos);

  auto binaryOrError = llvm::object::createBinary(objectPath.string());
  ASSERT_TRUE(static_cast<bool>(binaryOrError));
  const auto* object = llvm::dyn_cast<llvm::object::ObjectFile>(
      binaryOrError->getBinary());
  ASSERT_NE(object, nullptr);
  EXPECT_TRUE(object->isELF());
  EXPECT_TRUE(object->isRelocatableObject());
  EXPECT_EQ(object->getArch(), llvm::Triple::aarch64);

  std::filesystem::remove(assemblyPath);
  std::filesystem::remove(objectPath);
}
