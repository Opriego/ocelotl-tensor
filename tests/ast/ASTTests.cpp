// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/ast/AST.hpp"

#include <gtest/gtest.h>

using namespace ocelotl::ast;

TEST(ASTTest, RepresentsTensorDeclaration)
{
    TensorDecl declaration{
        "A",
        TensorType{
            "f32",
            {1024, 1024}
        },
        {1, 1, 0}
    };

    EXPECT_EQ(declaration.name, "A");
    EXPECT_EQ(declaration.type.elementType, "f32");

    ASSERT_EQ(declaration.type.shape.size(), 2);

    EXPECT_EQ(declaration.type.shape[0], 1024);
    EXPECT_EQ(declaration.type.shape[1], 1024);
}

TEST(ASTTest, RepresentsNestedCallExpression)
{
    auto add = std::make_shared<CallExpr>();

    add->callee = "add";
    add->location = {1, 6, 5};

    add->arguments.emplace_back(
        IdentifierExpr{
            "A",
            {1, 10, 9}
        }
    );

    add->arguments.emplace_back(
        IdentifierExpr{
            "B",
            {1, 13, 12}
        }
    );

    auto relu = std::make_shared<CallExpr>();

    relu->callee = "relu";
    relu->location = {1, 1, 0};
    relu->arguments.emplace_back(add);

    Expression expression = relu;

    ASSERT_TRUE(
        std::holds_alternative<std::shared_ptr<CallExpr>>(
            expression
        )
    );

    const auto root =
        std::get<std::shared_ptr<CallExpr>>(
            expression
        );

    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->callee, "relu");

    ASSERT_EQ(root->arguments.size(), 1);

    const auto nested =
        std::get<std::shared_ptr<CallExpr>>(
            root->arguments[0]
        );

    ASSERT_NE(nested, nullptr);

    EXPECT_EQ(nested->callee, "add");
    EXPECT_EQ(nested->arguments.size(), 2);
}
