# Copyright (c) 2011-2021 Columbia University, System Level Design Group
# SPDX-License-Identifier: Apache-2.0

#-----------------------------------------------------------
#                         ETHERNET
#-----------------------------------------------------------

# RX Clock
create_clock -period 40.000 [get_ports erx_clk]
set_false_path -reset_path -from [get_clocks {etx_clk erx_clk}] -to [get_clocks {mmcm_ps_clk_bufg_in*}]

set_propagated_clock [get_clocks erx_clk]
set_input_delay -clock [get_clocks erx_clk] 10 [all_inputs]

# TX Clock
create_clock -period 40.000 [get_ports etx_clk]
set_propagated_clock [get_clocks etx_clk]
set_output_delay -clock [get_clocks etx_clk] 5 [all_outputs]
set_input_delay  -clock [get_clocks etx_clk] 10 [all_inputs]

# RX/TX paths
set_max_delay -from [get_clocks -include_generated_clocks etx_clk] -to [get_clocks erx_clk] 40.000
set_max_delay -from [get_clocks erx_clk] -to [get_clocks -include_generated_clocks etx_clk] 40.000

# Other domains
set clkm_elab [get_clocks -of_objects [get_nets clkm]]
set clkm2_elab [get_clocks -of_objects [get_nets clkm_2]]
set refclk_elab [get_clocks -of_objects [get_nets chip_refclk]]

set_clock_groups -asynchronous -group [get_clocks erx_clk] -group [get_clocks $clkm_elab]
set_clock_groups -asynchronous -group [get_clocks erx_clk] -group [get_clocks $clkm2_elab]
set_clock_groups -asynchronous -group [get_clocks erx_clk] -group [get_clocks $refclk_elab]

set_clock_groups -asynchronous -group [get_clocks etx_clk] -group [get_clocks $clkm_elab]
set_clock_groups -asynchronous -group [get_clocks etx_clk] -group [get_clocks $clkm2_elab]
set_clock_groups -asynchronous -group [get_clocks etx_clk] -group [get_clocks $refclk_elab]
set_clock_groups -asynchronous -group [get_clocks etx_clk] -group [get_clocks {*_dmbi* *_mmi64}]
set_clock_groups -asynchronous -group [get_clocks etx_clk] -group [get_clocks {c0_* c1_*}]
set_clock_groups -asynchronous -group [get_clocks etx_clk] -group [get_clocks oserdes*]


