clear_all()

p4_pd.register_reset_all_version()
p4_pd.register_reset_all_agtr_time()
p4_pd.register_reset_all_register1()
p4_pd.register_reset_all_register2()
p4_pd.register_reset_all_register3()
p4_pd.register_reset_all_register4()
p4_pd.register_reset_all_register5()
p4_pd.register_reset_all_register6()
p4_pd.register_reset_all_register7()
p4_pd.register_reset_all_register8()
p4_pd.register_reset_all_register9()
p4_pd.register_reset_all_register10()
p4_pd.register_reset_all_register11()
p4_pd.register_reset_all_register12()
p4_pd.register_reset_all_register13()
p4_pd.register_reset_all_register14()
p4_pd.register_reset_all_register15()
p4_pd.register_reset_all_register16()
p4_pd.register_reset_all_register17()
p4_pd.register_reset_all_register18()
p4_pd.register_reset_all_register19()
p4_pd.register_reset_all_register20()
p4_pd.register_reset_all_register21()
p4_pd.register_reset_all_register22()
p4_pd.register_reset_all_register23()
p4_pd.register_reset_all_register24()
p4_pd.register_reset_all_register25()
p4_pd.register_reset_all_register26()
p4_pd.register_reset_all_register27()
p4_pd.register_reset_all_register28()
p4_pd.register_reset_all_register29()
p4_pd.register_reset_all_register30()
p4_pd.register_reset_all_register31()
p4_pd.register_reset_all_register32()


PS = "98:03:9b:03:54:20"
w1 = "b8:59:9f:1d:04:f2"
w2 = "b8:59:9f:0b:30:72"
w3 = "98:03:9b:03:46:50"
w4 = "b8:59:9f:02:0d:14"
w5 = "b8:59:9f:b0:2d:50"
w6 = "b8:59:9f:b0:2b:b0"
w7 = "b8:59:9f:b0:2b:b8"
w8 = "b8:59:9f:b0:2d:18"
w9 = "b8:59:9f:b0:2d:58"

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w1)),
    p4_pd.set_egr_action_spec_t(56)
)

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w2)),
    p4_pd.set_egr_action_spec_t(48)
)

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w3)),
    p4_pd.set_egr_action_spec_t(40)
)

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w4)),
    p4_pd.set_egr_action_spec_t(32)
)

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w5)),
    p4_pd.set_egr_action_spec_t(24)
)

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w6)),
    p4_pd.set_egr_action_spec_t(16)
)

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w7)),
    p4_pd.set_egr_action_spec_t(8)
)

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w8)),
    p4_pd.set_egr_action_spec_t(0)
)

p4_pd.forward_table_add_with_set_egr(
    p4_pd.forward_match_spec_t(macAddr_to_string(w9)),
    p4_pd.set_egr_action_spec_t(4)
)

p4_pd.set_position_by_version_table_add_with_set_position(
    p4_pd.set_position_by_version_match_spec_t(0),
    p4_pd.set_position_action_spec_t(1),
)

p4_pd.set_position_by_version_table_add_with_set_position(
    p4_pd.set_position_by_version_match_spec_t(1),
    p4_pd.set_position_action_spec_t(2),
)


try:
    # TODO: understand it
    # dont know why, but if group = input port,
    # then the packet followed by that packet will execute multicast
    # therefore make it 20, no 20th port is used.
    mcg1  = mc.mgrp_create(999)
except:
    print """
clean_all() does not yet support cleaning the PRE programming.
You need to restart the driver before running this script for the second time
"""
    quit()

node1 = mc.node_create(
    rid=999,
    #port_map=devports_to_mcbitmap([188]),
    port_map=devports_to_mcbitmap([56,48,40,32,24,16,8,0]),
    # port_map=devports_to_mcbitmap([56, 24]),
    lag_map=lags_to_mcbitmap(([]))
)
mc.associate_node(mcg1, node1, xid=0, xid_valid=False)

conn_mgr.complete_operations()
