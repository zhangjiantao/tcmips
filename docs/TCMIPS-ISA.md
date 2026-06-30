## 1. 简介

### 1. 架构简介

指令集基于 MIPS32r2 标准架构裁剪定制，整体遵循两个原则：

1. **硬件侧**：最大限度简化译码、执行逻辑，提升TPS。
2. **软件侧**：保持与通用MIPS编译器高兼容性，减少工具链定制改动成本。

裁剪移除指令清单：特权指令、原子操作指令、融合乘加指令、算术溢出异常类指令、CLZ/CLO、Trap陷阱指令、全部Likely类条件分支变体指令。

### 2. 架构特性

- **内存访问**：硬件原生支持非对齐地址加载/存储，无需软件手动对齐拆分。
- **异常与中断**：不实现异常、中断处理逻辑；已移除全部陷阱指令，以及 `ADD/ADDI/SUB` 等带算术溢出异常的运算指令。
- **分支延迟槽**：仅实现标准MIPS固定延迟槽逻辑，不支持Likely分支；所有跳转、条件分支后的延迟槽指令必定执行，与分支跳转结果无关。
- **SYSCALL**：取消原有操作系统内核调用语义，专用作外设硬件控制指令。
- **通用寄存器**：`$k0`、`$k1` 不再作为内核专用寄存器，程序可自由当作通用GPR使用。

---

## 2. 指令格式

<style>
.mips-table {
  width: 100%;
  border-collapse: collapse;
  font-family: monospace, Monaco, Consolas, "Courier New";
  font-size: 14px;
  line-height: 1.4;
}
.mips-table th, .mips-table td {
  border: 1px solid #ccc;
  padding: 6px 8px;
  vertical-align: middle;
}
.mips-table th {
  background: #f6f6f6;
  text-align: center;
}
.mips-table code {
  font-family: inherit;
  letter-spacing: 0.2px;
}
.odd-cat { background: #f9f9f9; }
.even-cat { background: #f2f2f2; }
</style>

<table role="table" class="mips-table">
  <thead>
    <tr>
      <th>格式 \ 位域</th>
      <th>[31 : 26] (6 bits)</th>
      <th>[25 : 21] (5 bits)</th>
      <th>[20 : 16] (5 bits)</th>
      <th>[15 : 11] (5 bits)</th>
      <th>[10 : 6] (5 bits)</th>
      <th>[5 : 0] (6 bits)</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td><b>R-Type</b> </td>
      <td>opcode</td>
      <td>rs</td>
      <td>rt</td>
      <td>rd</td>
      <td>sa</td>
      <td>function</td>
    </tr>
    <tr>
      <td><b>I-Type</b></td>
      <td>opcode</td>
      <td>rs</td>
      <td>rt</td>
      <td colspan="3" align="center" style="background:#f6f8fa;">&lt;--------- immediate / offset (16 bits) ---------&gt;</td>
    </tr>
    <tr>
      <td><b>J-Type</b></td>
      <td>opcode</td>
      <td colspan="5" align="center" style="background:#f6f8fa;">&lt;-------------------------------- instr_index (26 bits) ---------------------------------&gt;</td>
    </tr>
  </tbody>
</table>

------------------------------

## 3. 译码表

<style>
.mips-table {
  width: 100%;
  border-collapse: collapse;
  font-family: monospace, Monaco, Consolas, "Courier New";
  font-size: 14px;
  line-height: 1.4;
}
.mips-table th, .mips-table td {
  border: 1px solid #ccc;
  padding: 6px 8px;
  vertical-align: middle;
}
.mips-table th {
  background: #f6f6f6;
  text-align: center;
}
.mips-table code {
  font-family: inherit;
  letter-spacing: 0.2px;
}
.odd-cat { background: #f9f9f9; }
.even-cat { background: #f2f2f2; }
</style>

<table class="mips-table">
<thead>
<tr>
<th>指令类型</th>
<th>指令名称</th>
<th>Opcode</th>
<th>Function</th>
<th>Rs/Rt/Sa</th>
<th>操作</th>
<th>分支延迟槽行为</th>
</tr>
</thead>
<tbody>

<!-- 1 访存指令 浅1 -->
<tr class="odd-cat">
<td rowspan="8">访存指令</td>
<td>LB</td>
<td>4_0 (0x20)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = SignExtend(ReadMem(GPR[rs] + SignExtend(offset), 1))</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>LH</td>
<td>4_1 (0x21)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = SignExtend(ReadMem(GPR[rs] + SignExtend(offset), 2))</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>LW</td>
<td>4_3 (0x23)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = ReadMem(GPR[rs] + SignExtend(offset), 4)</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>LBU</td>
<td>4_4 (0x24)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = ZeroExtend(ReadMem(GPR[rs] + SignExtend(offset), 1))</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>LHU</td>
<td>4_5 (0x25)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = ZeroExtend(ReadMem(GPR[rs] + SignExtend(offset), 2))</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>SB</td>
<td>5_0 (0x28)</td>
<td>—</td>
<td>-</td>
<td><code>WriteMem(GPR[rs] + SignExtend(offset), 1, GPR[rt][7:0])</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>SH</td>
<td>5_1 (0x29)</td>
<td>—</td>
<td>-</td>
<td><code>WriteMem(GPR[rs] + SignExtend(offset), 2, GPR[rt][15:0])</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>SW</td>
<td>5_3 (0x2B)</td>
<td>—</td>
<td>-</td>
<td><code>WriteMem(GPR[rs] + SignExtend(offset), 4, GPR[rt])</code></td>
<td>无</td>
</tr>

<!-- 2 I-Type 运算 浅2 -->
<tr class="even-cat">
<td rowspan="7">I-Type 运算</td>
<td>ADDIU</td>
<td>1_1 (0x09)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = GPR[rs] + SignExtend(imm)</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>SLTI</td>
<td>1_2 (0x0A)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = ((int32_t)GPR[rs] < (int32_t)SignExtend(imm)) ? 1 : 0</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>SLTIU</td>
<td>1_3 (0x0B)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = ((uint32_t)GPR[rs] < (uint32_t)SignExtend(imm)) ? 1 : 0</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>ANDI</td>
<td>1_4 (0x0C)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = GPR[rs] & ZeroExtend(imm)</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>ORI</td>
<td>1_5 (0x0D)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = GPR[rs] | ZeroExtend(imm)</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>XORI</td>
<td>1_6 (0x0E)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = GPR[rs] ^ ZeroExtend(imm)</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>LUI</td>
<td>1_7 (0x0F)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[rt] = imm << 16</code></td>
<td>无</td>
</tr>

<!-- 3 I/J-Type 跳转 浅1 -->
<tr class="odd-cat">
<td rowspan="6">I/J-Type 跳转</td>
<td>J</td>
<td>0_2 (0x02)</td>
<td>—</td>
<td>-</td>
<td><code>PC_next = { (PC+4)[31:28], instr_index, 2'b00 }</code></td>
<td>有</td>
</tr>
<tr class="odd-cat">
<td>JAL</td>
<td>0_3 (0x03)</td>
<td>—</td>
<td>-</td>
<td><code>GPR[31] = PC + 8; PC_next = { (PC+4)[31:28], instr_index, 2'b00 }</code></td>
<td>有</td>
</tr>
<tr class="odd-cat">
<td>BEQ</td>
<td>0_4 (0x04)</td>
<td>—</td>
<td>-</td>
<td><code>PC_next = (GPR[rs] == GPR[rt]) ? (PC + 4 + (SignExtend(offset) << 2)) : (PC + 8)</code></td>
<td>有</td>
</tr>
<tr class="odd-cat">
<td>BNE</td>
<td>0_5 (0x05)</td>
<td>—</td>
<td>-</td>
<td><code>PC_next = (GPR[rs] != GPR[rt]) ? (PC + 4 + (SignExtend(offset) << 2)) : (PC + 8)</code></td>
<td>有</td>
</tr>
<tr class="odd-cat">
<td>BLEZ</td>
<td>0_6 (0x06)</td>
<td>—</td>
<td>-</td>
<td><code>PC_next = ((int32_t)GPR[rs] <= 0) ? (PC + 4 + (SignExtend(offset) << 2)) : (PC + 8)</code></td>
<td>有</td>
</tr>
<tr class="odd-cat">
<td>BGTZ</td>
<td>0_7 (0x07)</td>
<td>—</td>
<td>-</td>
<td><code>PC_next = ((int32_t)GPR[rs] > 0) ? (PC + 4 + (SignExtend(offset) << 2)) : (PC + 8)</code></td>
<td>有</td>
</tr>

<!-- 4 R-Type 跳转 浅2 -->
<tr class="even-cat">
<td rowspan="2">R-Type 跳转</td>
<td>JR</td>
<td>0_0 (0x00)</td>
<td>1_0 (0x08)</td>
<td>-</td>
<td><code>PC_next = GPR[rs]</code></td>
<td>有</td>
</tr>
<tr class="even-cat">
<td>JALR</td>
<td>0_0 (0x00)</td>
<td>1_1 (0x09)</td>
<td>-</td>
<td><code>GPR[rd] = PC + 8; PC_next = GPR[rs]</code></td>
<td>有</td>
</tr>

<!-- 5 REGIMM 类 浅1 -->
<tr class="odd-cat">
<td rowspan="2">REGIMM 类</td>
<td>BLTZ</td>
<td>0_1 (0x01)</td>
<td>-</td>
<td>rt == 0</td>
<td><code>PC_next = ((int32_t)GPR[rs] < 0) ? (PC + 4 + (SignExtend(offset) << 2)) : (PC + 8)</code></td>
<td>有</td>
</tr>
<tr class="odd-cat">
<td>BGEZ</td>
<td>0_1 (0x01)</td>
<td>-</td>
<td>rt == 1</td>
<td><code>PC_next = ((int32_t)GPR[rs] >= 0) ? (PC + 4 + (SignExtend(offset) << 2)) : (PC + 8)</code></td>
<td>有</td>
</tr>

<!-- 6 R-Type 移位 浅2 -->
<tr class="even-cat">
<td rowspan="8">R-Type 移位</td>
<td>SLL</td>
<td>0_0 (0x00)</td>
<td>0_0 (0x00)</td>
<td>-</td>
<td><code>GPR[rd] = GPR[rt] << sa</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>SRL</td>
<td>0_0 (0x00)</td>
<td>0_2 (0x02)</td>
<td>rs == 0</td>
<td><code>GPR[rd] = GPR[rt] >> sa</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>ROTR</td>
<td>0_0 (0x00)</td>
<td>0_2 (0x02)</td>
<td>rs == 1</td>
<td><code>GPR[rd] = (GPR[rt] >> sa) | (GPR[rt] << (32 - sa))</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>SRA</td>
<td>0_0 (0x00)</td>
<td>0_3 (0x03)</td>
<td>-</td>
<td><code>GPR[rd] = (int32_t)GPR[rt] >> sa</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>SLLV</td>
<td>0_0 (0x00)</td>
<td>0_4 (0x04)</td>
<td>-</td>
<td><code>GPR[rd] = GPR[rt] << GPR[rs][4:0]</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>SRLV</td>
<td>0_0 (0x00)</td>
<td>0_6 (0x06)</td>
<td>sa == 0</td>
<td><code>GPR[rd] = GPR[rt] >> GPR[rs][4:0]</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>ROTRV</td>
<td>0_0 (0x00)</td>
<td>0_6 (0x06)</td>
<td>sa == 1</td>
<td><code>GPR[rd]=(GPR[rt]>>GPR[rs][4:0])∣(GPR[rt]<<(32−GPR[rs][4:0]))</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>SRAV</td>
<td>0_0 (0x00)</td>
<td>0_7 (0x07)</td>
<td>-</td>
<td><code>GPR[rd] = (int32_t)GPR[rt] >> GPR[rs][4:0]</code></td>
<td>无</td>
</tr>

<!-- 7 R-Type 运算 浅1 -->
<tr class="odd-cat">
<td rowspan="12">R-Type 运算</td>
<td>MOVZ</td>
<td>0_0 (0x00)</td>
<td>1_2 (0x0A)</td>
<td>-</td>
<td><code>if (GPR[rt] == 0) GPR[rd] = GPR[rs]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>MOVN</td>
<td>0_0 (0x00)</td>
<td>1_3 (0x0B)</td>
<td>-</td>
<td><code>if (GPR[rt] != 0) GPR[rd] = GPR[rs]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>SYSCALL</td>
<td>0_0 (0x00)</td>
<td>1_4 (0x0C)</td>
<td>-</td>
<td><code>Peripheral Control</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>BREAK</td>
<td>0_0 (0x00)</td>
<td>1_5 (0x0D)</td>
<td>-</td>
<td><code>BreakPoint</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>ADDU</td>
<td>0_0 (0x00)</td>
<td>4_1 (0x21)</td>
<td>-</td>
<td><code>GPR[rd] = GPR[rs] + GPR[rt]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>SUBU</td>
<td>0_0 (0x00)</td>
<td>4_3 (0x23)</td>
<td>-</td>
<td><code>GPR[rd] = GPR[rs] - GPR[rt]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>AND</td>
<td>0_0 (0x00)</td>
<td>4_4 (0x24)</td>
<td>-</td>
<td><code>GPR[rd] = GPR[rs] & GPR[rt]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>OR</td>
<td>0_0 (0x00)</td>
<td>4_5 (0x25)</td>
<td>-</td>
<td><code>GPR[rd] = GPR[rs] | GPR[rt]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>XOR</td>
<td>0_0 (0x00)</td>
<td>4_6 (0x26)</td>
<td>-</td>
<td><code>GPR[rd] = GPR[rs] ^ GPR[rt]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>NOR</td>
<td>0_0 (0x00)</td>
<td>4_7 (0x27)</td>
<td>-</td>
<td><code>GPR[rd] = ~(GPR[rs] | GPR[rt])</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>SLT</td>
<td>0_0 (0x00)</td>
<td>5_2 (0x2A)</td>
<td>-</td>
<td><code>GPR[rd] = ((int32_t)GPR[rs] < (int32_t)GPR[rt]) ? 1 : 0</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>SLTU</td>
<td>0_0 (0x00)</td>
<td>5_3 (0x2B)</td>
<td>-</td>
<td><code>GPR[rd] = ((uint32_t)GPR[rs] < (uint32_t)GPR[rt]) ? 1 : 0</code></td>
<td>无</td>
</tr>

<!-- 8 HILO 访问 浅2 -->
<tr class="even-cat">
<td rowspan="4">HILO 访问</td>
<td>MFHI</td>
<td>0_0 (0x00)</td>
<td>2_0 (0x10)</td>
<td>-</td>
<td><code>GPR[rd] = HI</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>MTHI</td>
<td>0_0 (0x00)</td>
<td>2_1 (0x11)</td>
<td>-</td>
<td><code>HI = GPR[rs]</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>MFLO</td>
<td>0_0 (0x00)</td>
<td>2_2 (0x12)</td>
<td>-</td>
<td><code>GPR[rd] = LO</code></td>
<td>无</td>
</tr>
<tr class="even-cat">
<td>MTLO</td>
<td>0_0 (0x00)</td>
<td>2_3 (0x13)</td>
<td>-</td>
<td><code>LO = GPR[rs]</code></td>
<td>无</td>
</tr>

<!-- 9 乘除法运算 浅1 -->
<tr class="odd-cat">
<td rowspan="4">乘除法运算</td>
<td>MULT</td>
<td>0_0 (0x00)</td>
<td>3_0 (0x18)</td>
<td>-</td>
<td><code>{HI, LO} = (int64_t)(int32_t)GPR[rs] * (int64_t)(int32_t)GPR[rt]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>MULTU</td>
<td>0_0 (0x00)</td>
<td>3_1 (0x19)</td>
<td>-</td>
<td><code>{HI, LO} = (uint64_t)GPR[rs] * (uint64_t)GPR[rt]</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>DIV</td>
<td>0_0 (0x00)</td>
<td>3_2 (0x1A)</td>
<td>-</td>
<td><code>{HI, LO} = {GPR[rs]%GPR[rs], GPR[rs]/GPR[rs]} (有符号)</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>DIVU</td>
<td>0_0 (0x00)</td>
<td>3_3 (0x1B)</td>
<td>-</td>
<td><code>{HI, LO} = {GPR[rs]%GPR[rs], GPR[rs]/GPR[rs]} (无符号)</code></td>
<td>无</td>
</tr>

<!-- 10 SPECIAL2 类 浅2 -->
<tr class="even-cat">
<td rowspan="1">SPECIAL2 类</td>
<td>MUL</td>
<td>3_4 (0x1C)</td>
<td>0_2 (0x02)</td>
<td>-</td>
<td><code>GPR[rd] = ((int64_t)rs * (int64_t)rt)[31:0]</code></td>
<td>无</td>
</tr>

<!-- 11 SPECIAL3 类 浅1 -->
<tr class="odd-cat">
<td rowspan="3">SPECIAL3 类</td>
<td>BSHFL/WSBH</td>
<td>3_7 (0x1F)</td>
<td>4_0 (0x20) 且 sa == 0x02</td>
<td>-</td>
<td><code>GPR[rd] = { GPR[rt][23:16], GPR[rt][31:24], GPR[rt][7:0], GPR[rt][15:8] }</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>BSHFL/SEB</td>
<td>3_7 (0x1F)</td>
<td>4_0 (0x20) 且 sa == 0x10</td>
<td>-</td>
<td><code>GPR[rd] = SignExtend(GPR[rt][7:0])</code></td>
<td>无</td>
</tr>
<tr class="odd-cat">
<td>BSHFL/SEH</td>
<td>3_7 (0x1F)</td>
<td>4_0 (0x20) 且 sa == 0x18</td>
<td>-</td>
<td><code>GPR[rd] = SignExtend(GPR[rt][15:0])</code></td>
<td>无</td>
</tr>

</tbody>
</table>

