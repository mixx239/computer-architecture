<p align="center">
  <img src="https://img.shields.io/badge/C++-17/23-00599C?style=for-the-badge&logo=c%2B%2B"><br>
  <img src="https://img.shields.io/badge/OpenMP-28A745?style=for-the-badge"><br>
  <img src="https://img.shields.io/badge/RISC--V-Emulation-orange?style=for-the-badge&logo=riscv"><br>
  <img src="https://img.shields.io/badge/Logisim-blue?style=for-the-badge">
  <img src="https://img.shields.io/badge/Verilog-red?style=for-the-badge&logo=verilog">
</p>

<h1 align="center">Лабораторные работы по Архитектуре ЭВМ</h1>

<p align="center">
  В этом репозитории собраны проекты без упора на ООП, а с упором на алгоритмы, вычисления и архитектурные решения
</p>

---

### [**ЛР1 — Арифметика IEEE-754**](./Floating_point)  
Реализация основных арифметических операций IEEE-754, в т.ч. FMA и MAD, обработка half и single-precision, поддержка различных типов округления

---

### [**ЛР2 — Эмулятор RISC-V и моделирование кэша**](./RISC_V_Emulator)  
Эмуляция RV32I/RV32M, исполнение бинарного файла, подсистема кэширования (LRU, Bit-PLRU), статистика попаданий

---

### [**ЛР3 — Вычисление sqrt из half-precision**](./Sqrt2)  
Поразрядная реализация квадратного корня для half-precision на логических схемах + описание алгоритма на Verilog.

---

### [**ЛР4 — Оптимизация обработки изображений (PGM/PPM, OpenMP)**](./OMP_optimization)  
Улучшение контрастности изображения, многопоточность OpenMP + результаты тестов и подробный отчет