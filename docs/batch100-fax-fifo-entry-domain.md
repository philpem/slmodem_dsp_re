# FIFO entry capture correction

Two complete-TU cells. The first eight-cell write_fifo domain's late capture was inside EVERY loop iteration, while original 96910/96914 captures framer once after the entry count gate and loop edge targets96920 beyond it. Those differ under aliased element writes. This new independent witness licenses exactly the original gate+one-time capture+do/while postdecrement with witnessed postfix writer update. No other stores/types/locals reorder. Fixed exact frame source. Require raw baseline/full TU proof. No adjacent controls after miss.
