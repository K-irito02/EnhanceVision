// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>
#include "stubext.h"
#include "{header_file}"

{Namespace}

class {ClassName}Test : public ::testing::Test {
protected:
    void SetUp() override {
        stub.clear();
        obj = new {ClassName}();
        {SetUpStubs}
    }

    void TearDown() override {
        delete obj;
        stub.clear();
    }

    stub_ext::StubExt stub;
    {ClassName} *obj = nullptr;
};

{TestCases}

{NamespaceEnd}
