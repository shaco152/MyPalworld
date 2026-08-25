# 材料背包与建造系统：接口和蓝图配置指南

## 1. 已冻结的运行时架构

- `FItemDefinitionRow`：统一物品定义，`DT_ItemDefinitions` 的行名就是稳定 `ItemId`。
- `FItemStack`：`ItemId + Quantity` 堆叠；`UItemInventoryComponent` 负责最大堆叠、容量、原子配方消耗、复制和变化委托。
- `IHitReactInterface`：玩家攻击与采集资源之间唯一耦合点。
- `AMaterialSourceActor`：收到 HitReact 后生成掉落物；达到命中次数后隐藏，并用 Timer 重生。
- `AMaterialPickupActor`：地面物理掉落物；玩家 Overlap 后尽可能加入背包，背包满时保留未拾取数量。
- `FBuildingRecipeRow`：建筑清单、建筑 BP 类和材料成本；`DT_BuildingRecipes` 行名就是稳定 `BuildingId`。
- `UBuildingComponent`：`Inactive → CatalogOpen → Previewing` 状态机；Timer 刷新虚影位置，滚轮事件旋转，左键确认，服务端重新校验地面/坡度/距离/阻挡/材料后生成。
- `ABuildingBase`：建筑稳定 ID、持久化 GUID、PlacementBounds 和预览材质边界；后续箱子/门/床/制作台应优先挂组件扩展。
- UI：`UMaterialInventoryWidget → UMaterialSlotWidget`，`UBuildMenuWidget → UBuildRecipeEntryWidget`。布局与样式全部在 WBP，C++ 只更新数据；动态数量只创建 WBP 模板。

整个新模块没有 Actor Tick、Component Tick 或 Widget NativeTick。资源重生与虚影刷新使用 `FTimerHandle`，背包/UI 使用委托。

## 2. 创建物品定义表

1. 内容浏览器进入 `Mine/Data`（没有就新建）。
2. 右键 → Miscellaneous → Data Table。
3. Row Structure 选择 `ItemDefinitionRow`，命名 `DT_ItemDefinitions`。
4. 添加测试行；**行名大小写必须与资源点和建筑配方中的 ItemId 完全一致**。

建议测试数据：

| Row Name（ItemId） | Display Name | Category | Max Stack Size | Icon |
|---|---|---|---:|---|
| `Wood` | 木材 | Material | 99 | 木材图标 |
| `Stone` | 石头 | Material | 99 | 石头图标 |

## 3. 创建掉落物和资源点 BP

### 3.1 地面掉落物

1. 创建 Blueprint Class，父类选 `MaterialPickupActor`，命名 `BP_MaterialPickup_Wood`。
2. 打开 Components，选择继承的 `PickupMesh`，设置木材 Static Mesh；只配置外观，不写拾取蓝图逻辑。
3. 如需石头使用不同模型，再建 `BP_MaterialPickup_Stone`。
4. `PickupCollision` 已由 C++ 配置为：WorldStatic/WorldDynamic Block、Pawn Overlap、物理模拟和重力；通常无需修改。

### 3.2 资源点

1. 创建 Blueprint Class，父类选 `MaterialSourceActor`，命名 `BP_Resource_Tree`。
2. 选择继承的 `SourceMesh`，设置树模型。
3. Collision Preset 可用 BlockAll；至少必须保证 `Visibility = Block`，否则普攻的资源 HitReact 扫描无法命中。
4. Class Defaults → `Material Source`：
   - `Material Item Id = Wood`
   - `Drop Quantity Min = 1`
   - `Drop Quantity Max = 3`
   - `Hits Until Depleted = 3`
   - `Pickup Class = BP_MaterialPickup_Wood`
   - `Respawn Delay = 30`
5. 可选：在 Event Graph 实现 `On Hit React Visual` / `On Depleted Visual`，只播放震动、音效或粒子，不能在这里另写掉落和背包逻辑。
6. 将 `BP_Resource_Tree` 拖进 `TestMap`。

石矿同理：`Material Item Id = Stone`，Pickup Class 改为石头掉落物。

## 4. 创建建筑 BP 和预览材质

### 4.1 预览材质

创建两个材质，例如：

- `M_BuildPreview_Valid`：绿色、半透明（或 Masked + DitherTemporalAA）。
- `M_BuildPreview_Invalid`：红色、半透明（或 Masked + DitherTemporalAA）。

建筑模型必须至少有一个材质槽。C++ 会把预览材质覆盖到虚影的每个 Mesh 材质槽；正式建筑会重新生成，不会继承虚影材质。

### 4.2 建筑蓝图

1. 创建 Blueprint Class，父类选 `BuildingBase`，例如 `BP_Building_Foundation`。
2. 在 Components 中添加 Static Mesh，挂到继承的 `SceneRoot`，设置地基模型及正式碰撞。
3. 选择继承的 `PlacementBounds`，调整 Box Extent 和 Relative Location，使盒体覆盖模型占地；盒体底面尽量贴着建筑底面。
4. Class Defaults → `Building | Preview`：
   - `Valid Preview Material = M_BuildPreview_Valid`
   - `Invalid Preview Material = M_BuildPreview_Invalid`
5. BP 只挂资产，不创建 Tick、不写放置或扣材料逻辑。

墙、门框等其他建筑都从 `BuildingBase` 各建一个 BP，并分别调整 `PlacementBounds`。

## 5. 创建建筑配方表

1. 右键 → Miscellaneous → Data Table。
2. Row Structure 选择 `BuildingRecipeRow`，命名 `DT_BuildingRecipes`。
3. 添加测试行；行名就是稳定 `BuildingId`。

示例：

| Row Name | Display Name | Building Class | Material Costs | Placement Distance | Placement Z Offset |
|---|---|---|---|---:|---:|
| `Foundation_Wood` | 木质地基 | `BP_Building_Foundation` | `Wood x 5` | 400 | 0 |

`Placement Z Offset` 用于修正模型枢轴不在底部的问题；先保持 0，若虚影沉入或浮在地面再微调。

## 6. 配置玩家组件

打开当前玩家蓝图（项目现有 `BP_Player` 或实际 GameMode 使用的 PlayerCharacter 子类）：

1. 选择继承组件 `ItemInventory`：
   - `Item Definitions = DT_ItemDefinitions`
   - `Stack Capacity = 24`（可调）
   - 可选在 `Stacks` 预填 `Wood x 20 / Stone x 20` 做建造测试。
2. 选择继承组件 `BuildingComponent`：
   - `Building Catalog = DT_BuildingRecipes`
   - `Placement Refresh Interval = 0.05`
   - `Ground Trace Half Height = 800`
   - `Max Ground Slope Degrees = 30`
   - `Rotation Step Degrees = 15`

## 7. 创建四个 WBP

项目已设置 `bAuthorizeAutomaticWidgetVariableCreation=False`，所以以下控件都必须手动勾选 **Is Variable**，名称必须完全一致。

### 7.1 `WBP_MaterialSlot`

Parent Class：`MaterialSlotWidget`

设计器内自由排版，提供：

- Image：`ItemIcon`
- TextBlock：`ItemNameText`
- TextBlock：`QuantityText`

建议根节点用 SizeBox 固定单格尺寸；颜色、字体、边框全部在设计器设置。

### 7.2 `WBP_MaterialInventory`

Parent Class：`MaterialInventoryWidget`

提供：

- WrapBox：`MaterialGrid`
- TextBlock：`CapacityText`
- TextBlock：`EmptyText`（默认文字可写“暂无材料”）

Class Defaults：`Slot Widget Class = WBP_MaterialSlot`。

### 7.3 `WBP_BuildRecipeEntry`

Parent Class：`BuildRecipeEntryWidget`

提供：

- Button：`SelectButton`
- Image：`BuildingIcon`（放在 SelectButton 内，图案即可点击）
- TextBlock：`BuildingNameText`
- TextBlock：`CostText`
- TextBlock：`AvailabilityText`

### 7.4 `WBP_BuildMenu`

Parent Class：`BuildMenuWidget`

提供：

- WrapBox：`RecipeGrid`
- TextBlock：`EmptyCatalogText`（默认文字可写“建筑清单为空”）

Class Defaults：`Entry Widget Class = WBP_BuildRecipeEntry`。

## 8. Enhanced Input 与 PlayerController

在 `Mine/Input` 创建：

1. `IA_MaterialInventory`：Value Type = Digital (Bool)。
2. `IA_BuildMode`：Value Type = Digital (Bool)。
3. `IA_BuildRotate`：Value Type = Axis1D (Float)。

打开现有 `IMC_Player` 添加：

- `IA_MaterialInventory` → `B`
- `IA_BuildMode` → `C`
- `IA_BuildRotate` → `Mouse Wheel Axis`

打开实际使用的 `BP_PC`，Class Defaults：

- `Material Inventory Action = IA_MaterialInventory`
- `Build Mode Action = IA_BuildMode`
- `Build Rotate Action = IA_BuildRotate`
- `Material Inventory Widget Class = WBP_MaterialInventory`
- `Build Menu Widget Class = WBP_BuildMenu`

无需新增蓝图输入事件；所有绑定都在 C++ `PalPlayerController::SetupInputComponent`。

## 9. 操作与验收

1. PIE 后左键攻击资源点：每次 HitReact 生成一个地面材料 Actor；资源点达到命中次数后隐藏，30 秒后重生。
2. 玩家走近掉落物：Overlap 自动拾取；屏幕显示“获得 Wood xN”。
3. 按 B：材料背包打开，相同 ItemId 会按 `MaxStackSize` 堆叠；B 或 Esc 关闭。
4. 按 C：出现建筑清单和鼠标；点击某个建筑图案。
5. 清单关闭，玩家正前方出现对应建筑虚影；WASD 移动和鼠标转向时虚影跟随。
6. 滚轮：每格按 `Rotation Step Degrees` 改变 Yaw。
7. 绿色虚影时左键：服务端重新校验并扣材料，生成正式建筑；保持当前建筑选择，可连续建造。
8. 红色虚影：可能是材料不足、坡度过大、没有地面、距离异常或 PlacementBounds 与其他物体重叠；左键不会扣材料。
9. C 或 Esc：销毁虚影并退出建造模式。

建议检查 `Saved/Logs/FinalProject.log` 中以下 `[诊断]` 链路：

- `玩家攻击触发资源 HitReact`
- `资源命中反应`
- `材料拾取`
- `ItemInventory 加入`
- `选择建筑 ... 虚影已生成`
- `建造成功` 或 `服务端拒绝建造`

## 10. Rider 文件索引

本模块新增了多个 C++ 文件。若 Rider 显示 `no index`，先按 `Ctrl+Alt+Y` 同步；仍无效则关闭 UnrealEditor 后重新生成项目文件或重开工程。
