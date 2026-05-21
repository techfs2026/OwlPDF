# MuQt 架构设计

## 整体架构

项目采用分层架构，从高到低依次为：**UI 层 → Session 层 → Handler / Cache / Renderer 层 → Model / Manager / Tool 层**，此外还有横切的 Util 工具包。

设计原则：上层依赖下层，不跨层调用（目标方向，代码持续朝此调整）。

<img src="docs/images/MuQt-architecture.png" alt="架构图" width="620px" height="600px"/>

---

## 各层说明

### UI 层

负责界面布局，响应用户事件，接收 Session 层信号更新界面。

- **MainWindow**：主窗口，包含菜单栏、工具栏、Tab 页
- **PDFDocumentTab**：单个 Tab 页，包含导航栏（NavigationPanel）和页面（PDFPageWidget）
- **NavigationPanel**：导航栏，包含大纲（OutlineWidget）和缩略图（ThumbnailWidget）
- **SearchWidget**、**OutlineDialog**：独立小组件

---

### Session 层

应用最核心的一层。UI 层的所有交互全部委托给 Session，由 Session 根据职责分发给对应 Handler；Session 同时向 UI 层发送信号驱动界面更新。

- **PDFDocumentSession**：核心对象，管理 Handler、State、Renderer、Cache 的生命周期
- **PDFDocumentState**：PDF 核心状态数据（页码、缩放、显示模式等）
- 每个 Tab 页拥有独立的 Session，多 Tab 之间数据完全隔离

---

### Handler 层

Session 不直接处理业务，而是分发给具体 Handler。Handler 是真正干活的地方。

| Handler | 职责 |
|---------|------|
| **PDFContentHandler** | 打开 / 关闭 PDF，加载大纲和缩略图数据 |
| **PDFViewHandler** | 管理视图状态：页码跳转、缩放、显示模式切换 |
| **PDFInteractionHandler** | 用户交互：文本选择、搜索、链接跳转 |

Handler 不持有状态和数据，状态来自 Session 的 State，数据来自 Manager 层。

---

### Cache 层

Session 管理两类缓存：

- **TextCacheManager**：文本缓存，为搜索、文本选择及未来批注功能提供数据
- **PageCacheManager**：页面缓存，存储已渲染页面，减少重复渲染，提升流畅度

---

### Renderer 层

封装 MuPDF API，为上层提供统一的渲染接口。

MuPDF 的 context / document 不能跨线程共享，因此每个线程（主线程 / 线程池中的线程）持有独立的 Renderer 实例。

---

### Manager 层

为 Handler 提供数据支撑，Handler 本身不持有业务数据。

- **OutlineManager**：大纲数据
- **ThumbnailManagerV2**：缩略图数据
- **SearchManager**：搜索功能

---

### Model 层 / Tool 层

辅助层，提供数据建模和通用工具，供其他模块复用。非 MVC / MVVM 意义上的 Model。

---

## 性能优化

### 缩略图加载策略

针对不同规模的 PDF 采用差异化加载方案，优先解决"卡顿"问题：

| PDF 规模 | 页数 | 策略 |
|---------|------|------|
| 小型 | < 50 页 | 同步渲染，速度足够快 |
| 中型 | 50 – 400 页 | 线程池批量加载（4 线程 × 5 页/批 = 一次性加载 20 页） |
| 大型 | > 400 页 | 按需加载：优先可见区 → 滚动时分批加载 → 停止滚动 / 跳页时补全 |

> 当前不足：大中小 PDF 的划分阈值和批次大小均为经验值，尚未基于实测数据调优。

### 页面缓存

渲染完成的页面存入 PageCacheManager，翻页时优先命中缓存，避免重复渲染。

### 全文文本缓存

PDF 文本内容在打开时异步预加载到 TextCacheManager，搜索和文本选择操作直接读取缓存，无需实时解析。