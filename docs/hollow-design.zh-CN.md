# Hollow 设计文档

## 目标

Hollow 不是一个功能完整的处理器，而是一个为 TraceRTL 开发迭代服务的最小 DUT。它的目标是：

1. 在 standalone 模式下快速生成 RTL 并完成 trace 驱动。
2. 保持和现有 TraceRTL DPI 接口兼容。
3. 在逻辑上模拟前端批量取指、后端较窄输出、以及 redirect 恢复行为。
4. 作为后续回挂 XiangShan 子仓库时的独立替身实现。

## 目录结构

当前 Hollow 代码位于 `src/main/scala/hollow/`，按职责拆分为多个文件：

- `HollowTopParams.scala`：参数定义与约束
- `HollowGeneratorConfig.scala`：Mill 入口参数解析
- `HollowTraceEntry.scala`：trace entry 类型与 PC 推导辅助逻辑
- `HollowPortRecords.scala`：带命名 slot 端口的 Record IO 定义
- `HollowInlineSv.scala`：内联 SystemVerilog 源码生成
- `HollowTraceFetch.scala`：Trace fetch BlackBox
- `HollowTraceReporter.scala`：drive/commit/redirect 上报 BlackBox
- `HollowAssertions.scala`：时序连续性断言
- `HollowTop.scala`：顶层队列与控制逻辑

## 参数体系

参数统一由 `HollowTopParams` 管理，而不是把常量散落在单个文件和内联字符串中。当前默认参数包括：

- `fetchWidth = 8`
- `issueWidth = 4`
- `fetchQueueDepth = 32`
- `commitQueueDepth = 32`
- `fetchLowWatermark = 8`
- `redirectLfsrWidth = 16`
- `redirectCompareBits = 6`
- `redirectCompareValue = 0x3f`

这些参数不仅被 Chisel 顶层使用，也会传入 BlackBox 的 SystemVerilog 代码生成函数，用于生成 `localparam` 和端口宽度，减少硬编码数字。

## 顶层结构

`HollowTop` 是 standalone 模式下的 Verilog 顶层模块。它由三部分组成：

1. `HollowTraceFetch`
2. `fetchQueue` 与 `commitQueue`
3. `HollowTraceReporter`

整体数据流如下：

1. 当 `fetchQueue` 水位低于门限时，`HollowTraceFetch` 从 TraceRTL refill 一批指令。
2. refill 得到的 8 条指令被放入 `fetchQueue`。
3. 每周期最多从 `fetchQueue` 取出 4 条，作为 drive 输出，同时进入 `commitQueue`。
4. `commitQueue` 中最多有 4 条指令在同一周期作为 commit 候选。
5. `HollowTraceReporter` 负责把 drive、commit 和 redirect 通过 DPI 汇报给 TraceRTL。

## HollowTraceFetch

`HollowTraceFetch` 的职责是调用 `trace_read_one_instr`。和旧实现不同，这一版有两个明显变化：

1. IO 按 slot 拆开，而不是把多条指令压成大总线。
2. 内联 SystemVerilog 使用参数生成，而不是直接手写每个 slot 的逻辑。

对每个 slot，Fetch 都输出：

- `valid_i`
- `pc_i`
- `instr_i`
- `target_i`
- `exception_i`
- `branch_type_i`
- `branch_taken_i`
- `inst_id_i`

这样做的主要目的是提高 debug 可读性，特别是在波形里按 slot 直接看字段。

## Queue 模型

Hollow 内部有两个队列：

- `fetchQueue`
- `commitQueue`

两者都不是 FIRRTL queue primitive，而是显式的寄存器数组加计数器，这样更便于做顺序约束检查和 redirect flush。

### fetchQueue

- 接收 `HollowTraceFetch` refill 的 8 条指令
- 每周期向 issue 侧提供最多 4 条
- 通过低水位控制 refill，避免溢出

### commitQueue

- 保存已经 drive 过、等待 commit 的指令
- 每周期最多提交 4 条
- 如果队头是异常类指令，则不按普通指令提交
- 如果队头是分支且触发误预测，则由 redirect 逻辑接管

## Redirect 模型

Hollow 并不做真实的分支预测，而是用一个 LFSR 产生低概率误预测事件。

触发条件：

- `commitQueue` 队头有效
- 队头不是异常
- 队头是分支类指令
- LFSR 低位匹配 `redirectCompareValue`

触发后行为：

1. 当周期通过 `trace_redirect(inst_id)` 向 TraceRTL 报告 redirect。
2. 本地 `fetchQueue` 和 `commitQueue` 被清空。
3. 下一轮由 TraceRTL 重新投喂从 redirect 点开始的指令流。

这个模型的目标不是复现 XiangShan 的真实前端，而是给 TraceRTL 的 redirect 路径提供持续覆盖。

## 断言设计

当前在 `HollowAssertions.scala` 中加入了连续性断言。它会检查相邻两条有效指令的两个关系：

1. 下一条 `instId == 上一条 instId + 1`
2. 下一条 PC 符合上一条指令的控制流结果

PC 关系规则：

- 若上一条是异常或已 taken 的分支，则下一条 PC 应为 `target`
- 否则下一条 PC 应为 `pc + 4` 或 `pc + 2`
- 指令长度由 `instr[1:0]` 判断是否为压缩指令

这些断言当前作用于：

- 一次 refill 得到的 `fetch-response`
- `fetchQueue` 中的连续有效项
- `commitQueue` 中的连续有效项

redirect 发生时顶层会 flush 队列，因此不会要求 flush 前后的两段序列仍然连续。

## 顶层命名与构建

standalone 模式的顶层模块现在是 `HollowTop`，对应：

- Mill 入口：`hollow.HollowTop`
- Verilog 顶层：`HollowTop`
- Verilator 生成类：`VHollowTop`

XiangShan 模式仍然保留原始的 `SimTop` 流程，因此 Makefile 里根据 `MODE` 切换：

- `MODE=standalone` -> Standalone 模式 -> `HollowTop`
- `MODE=xiangshan` -> `SimTop`

## 当前验证状态

已验证 standalone 模式：

```bash
make clean
make emu
make emu-run TRACE_FILE=~/work/trace/nemu-trace-v4-other/coremark-1t.trace.zstd
```

验证结果：

- `TRACE FINISHED`
- `338,675 committed instructions`
- `85,780 cycles`
- redirect replay 正常发生

XiangShan 模式目前只验证了命令展开和构建入口切换，尚未在真实 subrepo 布局下完成完整运行验证。