# Virtual modem clearing loop widths

902f47fa fixed profile. FAXVMI_control's original frame and FIFO clearing
loops increment then zero-extend AX and compare word bounds; current local
i is int. One unsigned-short local control. FAXVMI_create uses equivalent
frame/FIFO walks and two fixed-buffer walks; independently transfer that
local width with all limits/guards/stores fixed. Baseline plus control-only,
constructor-only and both cells. No layout/header, pointer ownership or
arbitrary local-order changes. All count boundaries remain unsigned words
as members are declared. Full-TU audit and exact-name losses required.
