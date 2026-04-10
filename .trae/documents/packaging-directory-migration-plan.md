# 打包目录整合迁移计划

## 目标

将项目中分散的打包相关目录整合到一个统一的 `packaging/` 目录下，提高项目结构的清晰度和可维护性。

## 当前状态

### 需要迁移的目录

| 原路径 | 内容 |
|--------|------|
| `installer/` | NSIS 安装程序脚本、许可证文件、README |
| `package/` | 打包输出目录（安装程序和便携版） |
| `portable/` | 便携版启动脚本 |
| `scripts/` | 打包构建和验证脚本 |

### 当前目录结构

```
EnhanceVision/
├── installer/
│   ├── README.md
│   ├── setup.nsi
│   └── license/
│       ├── license_en.txt
│       └── license_zh.txt
├── package/
│   ├── EnhanceVision-v0.1.0-windows-x64/
│   └── EnhanceVision-v0.1.0-windows-x64-portable.zip
├── portable/
│   └── start.vbs
└── scripts/
    ├── build-package.ps1
    └── verify-package.ps1
```

## 目标结构

```
EnhanceVision/
└── packaging/
    ├── installer/          # NSIS 安装程序
    │   ├── README.md
    │   ├── setup.nsi
    │   ├── license/
    │   │   ├── license_en.txt
    │   │   └── license_zh.txt
    │   ├── resources/      # (待创建：品牌图片)
    │   └── redist/         # (待创建：运行库)
    ├── output/             # 打包输出
    │   ├── EnhanceVision-v0.1.0-windows-x64/
    │   └── EnhanceVision-v0.1.0-windows-x64-portable.zip
    ├── portable/           # 便携版
    │   └── start.vbs
    └── scripts/            # 打包脚本
        ├── build-package.ps1
        └── verify-package.ps1
```

## 需要更新路径引用的文件

### 1. 核心脚本文件

| 文件 | 需要更新的路径引用 |
|------|-------------------|
| `packaging/scripts/build-package.ps1` | `package\` → `packaging\output\`<br>`installer\` → `packaging\installer\`<br>`portable\` → `packaging\portable\`<br>`scripts\` → `packaging\scripts\` |
| `packaging/scripts/verify-package.ps1` | `package\` → `packaging\output\` |

### 2. NSIS 安装程序脚本

| 文件 | 需要更新的路径引用 |
|------|-------------------|
| `packaging/installer/setup.nsi` | `..\package\` → `..\output\`<br>`..\installer\` → `..\installer\`（保持不变）<br>`..\resources\icons\` → `..\..\resources\icons\` |

### 3. 技能文档

| 文件 | 需要更新的路径引用 |
|------|-------------------|
| `.trae/skills/packaging-workflow/SKILL.md` | 所有路径引用更新为新结构 |

### 4. 用户文档

| 文件 | 需要更新的路径引用 |
|------|-------------------|
| `docs/preReleasePrep/03-packaging-guide_CN.md` | 所有路径引用更新为新结构 |
| `docs/preReleasePrep/03-packaging-guide_EN.md` | 所有路径引用更新为新结构 |
| `packaging/installer/README.md` | 路径引用更新 |

## 执行步骤

### 阶段一：创建新目录结构

1. 创建 `packaging/` 根目录
2. 创建子目录结构：
   - `packaging/installer/`
   - `packaging/installer/license/`
   - `packaging/output/`
   - `packaging/portable/`
   - `packaging/scripts/`

### 阶段二：迁移文件

1. 迁移 `installer/` 目录内容到 `packaging/installer/`
2. 迁移 `package/` 目录内容到 `packaging/output/`
3. 迁移 `portable/` 目录内容到 `packaging/portable/`
4. 迁移 `scripts/` 目录内容到 `packaging/scripts/`

### 阶段三：更新路径引用

#### 3.1 更新 `packaging/scripts/build-package.ps1`

- 第 20 行：`$packageDir = "package\..."` → `$packageDir = "packaging\output\..."`
- 第 74 行：`.\scripts\verify-package.ps1` → `.\packaging\scripts\verify-package.ps1`
- 第 79 行：`installer\setup.nsi` → `packaging\installer\setup.nsi`
- 第 84-93 行：`package\` → `packaging\output\`
- 第 88 行：`portable\start.vbs` → `packaging\portable\start.vbs`

#### 3.2 更新 `packaging/scripts/verify-package.ps1`

- 第 5 行：`$packageDir = "package\..."` → `$packageDir = "packaging\output\..."`

#### 3.3 更新 `packaging/installer/setup.nsi`

- 第 36 行：`OutFile "..\package\..."` → `OutFile "..\output\..."`
- 第 47-48 行：`..\resources\icons\` → `..\..\resources\icons\`
- 第 82-83 行：`..\installer\license\` → `license\`（相对路径简化）
- 第 279 行：`..\package\...` → `..\output\...`
- 第 283 行：`..\installer\redist\` → `redist\`

#### 3.4 更新 `.trae/skills/packaging-workflow/SKILL.md`

更新所有路径引用：
- `scripts\` → `packaging\scripts\`
- `installer\` → `packaging\installer\`
- `package\` → `packaging\output\`
- `portable\` → `packaging\portable\`

#### 3.5 更新文档文件

更新 `docs/preReleasePrep/03-packaging-guide_CN.md` 和 `docs/preReleasePrep/03-packaging-guide_EN.md` 中的所有路径引用。

更新 `packaging/installer/README.md` 中的路径引用。

### 阶段四：清理旧目录

确认所有文件已正确迁移且路径引用已更新后，删除旧目录：

1. 删除 `installer/` 目录
2. 删除 `package/` 目录
3. 删除 `portable/` 目录
4. 删除 `scripts/` 目录

### 阶段五：验证

1. 运行 `packaging/scripts/build-package.ps1` 验证打包流程
2. 检查生成的安装程序和便携版是否正常
3. 验证所有文档链接是否正确

## 风险评估

### 低风险

- 文件迁移操作简单，可逆
- 路径引用更新为文本替换，易于验证

### 需要注意

- NSIS 脚本中的相对路径需要仔细计算
- 确保所有文档中的路径引用都已更新
- 打包输出目录重命名为 `output` 更符合语义

## 回滚方案

如果迁移出现问题，可以通过 Git 回退到迁移前的状态。

## 预期收益

1. **结构清晰**：所有打包相关文件集中在一个目录
2. **易于维护**：修改打包流程时只需关注 `packaging/` 目录
3. **语义明确**：`output/` 比 `package/` 更能表达"输出"的含义
4. **减少根目录混乱**：根目录更加整洁
