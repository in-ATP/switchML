#include <tofino/intrinsic_metadata.p4>
#include <tofino/stateful_alu_blackbox.p4>
#include <tofino/constants.p4>
#include "includes/headers.p4"
#include "includes/registers.p4"
#include "includes/parser.p4"
#include "includes/tables.p4"
#include "includes/actions.p4"

/*
 * P4PS
 * /

/*************************************************************************
 ***********************  R E G I S T E R  *******************************
 *************************************************************************/

blackbox stateful_alu _check_counter_ {
    reg: agtr_time;
	// bitmap = 0 imply =, !=0 imply +=
    condition_lo           :  register_lo + 1 == p4ml.agtr_time;
    output_dst             :  mdata.bitmap; 

    update_lo_1_predicate  :  condition_lo;
    update_lo_1_value      :  0;

    update_lo_2_predicate  :  not condition_lo;
    update_lo_2_value      :  register_lo + 1;

    output_value           :  register_lo;
}

blackbox stateful_alu _read_counter_ {
    reg: agtr_time;

    output_dst             :  mdata.bitmap; 
    output_value           :  register_lo;
}


/*************************************************************************
 **************  I N G R E S S   P R O C E S S I N G   *******************
 *************************************************************************/

/*
 * Actions 
 */


action multicast(group) {
    modify_field(ig_intr_md_for_tm.mcast_grp_a, group);
}


action _check_counter() {
    _check_counter_.execute_stateful_alu(p4ml.agtr);
	// agtr = ith Agtr
}

action _read_counter(){
    _read_counter_.execute_stateful_alu(p4ml.agtr);
}

table check_counter {
    actions {
        _check_counter;//mdata.seen == 0;
    }
    default_action: _check_counter;
    size : 1;
}
table read_counter {
    actions {
		_read_counter; //mdata.seen == 1;
    }
    default_action: _read_counter;
    size : 1;
}

table drop_table {
    actions {
        drop_pkt;
    }
    default_action: drop_pkt();
}

action drop_pkt() {
    drop();
}

action increase_counter(){
	add(mdata.current_counter, mdata.bitmap,  1);
}

table increase_counter_table {
    actions {
		increase_counter; //mdata.seen == 1;
    }
    default_action: increase_counter;
    size : 1;
}

action set_ack(){
	modify_field(p4ml.isACK, 1);
}
table set_ack_table {
    actions {
        set_ack;
    }
    default_action: set_ack();
    size : 1;
}

register version {
    width : 8;
    instance_count : 80000;
}

blackbox stateful_alu check_version {
    reg: version;
    condition_hi           :  register_lo != mdata.position;

    update_lo_1_predicate  :  condition_hi;
    update_lo_1_value      :  mdata.position;
    output_predicate 	   :  condition_hi;
    output_dst             :  mdata.isAggregate; 
    output_value           :  mdata.position;
}

action check_current_version(){
	check_version.execute_stateful_alu(p4ml.versionIndex);
	// versionIndex = Number of worker * Number of Agtr + ith worker
}

table check_current_agtr_version {
    actions {
        check_current_version;
    }
    default_action: check_current_version();
    size : 1;
}

// if version == 0, value = 2b 01
// if version == 1, value = 2b 10
action set_position(value){
	modify_field(mdata.position, value);
	modify_field(mdata.isAggregate, 0);
}

table set_position_by_version{
	reads{
		p4ml.version: exact;
	}
    actions {
    	set_position;
	}
    default_action: nop;
    size : 8;

}

table outPort_table {
    reads {
        // useless here, just can't use default action for variable
        p4ml.isACK : exact;
    }
    actions {
        set_egr;
		nop;
    }
}

action echo_back(){
 modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);	
}

table echo_back_table{
	actions{
		echo_back;	
	}
	default_action: echo_back;
	size: 1;
}
table multicast_table {
    actions {
        multicast;
    }
    // refer to run_pd_rpc/setup.py
    default_action: multicast(999);
}

action nop()
{
}

action set_egr(egress_spec) {
	 modify_field(ig_intr_md_for_tm.ucast_egress_port, egress_spec);
	// increase_p4ml_counter.execute_stateful_alu(ig_intr_md.ingress_port);
}


table forward {
    reads {
        ethernet.dstAddr : exact;
    }
    actions {
        set_egr; nop;
    }
}

control ingress 
{


    if (valid(p4ml_entries)) {
	    apply(set_position_by_version);
	    apply(check_current_agtr_version);
		// If aggreagte is needed
	    if(mdata.isAggregate == mdata.position) {
		    apply(check_counter);
		    apply(increase_counter_table);

			if (mdata.current_counter == p4ml.agtr_time) {
				apply(processEntry1andWriteToPacket);
				apply(processEntry2andWriteToPacket);
				apply(processEntry3andWriteToPacket);
				apply(processEntry4andWriteToPacket);
				apply(processEntry5andWriteToPacket);
				apply(processEntry6andWriteToPacket);
				apply(processEntry7andWriteToPacket);
				apply(processEntry8andWriteToPacket);
				apply(processEntry9andWriteToPacket);
				apply(processEntry10andWriteToPacket);
				apply(processEntry11andWriteToPacket);
				apply(processEntry12andWriteToPacket);
				apply(processEntry13andWriteToPacket);
				apply(processEntry14andWriteToPacket);
				apply(processEntry15andWriteToPacket);
				apply(processEntry16andWriteToPacket);
				apply(processEntry17andWriteToPacket);
				apply(processEntry18andWriteToPacket);
				apply(processEntry19andWriteToPacket);
				apply(processEntry20andWriteToPacket);
				apply(processEntry21andWriteToPacket);
				apply(processEntry22andWriteToPacket);
				apply(processEntry23andWriteToPacket);
				apply(processEntry24andWriteToPacket);
				apply(processEntry25andWriteToPacket);
				apply(processEntry26andWriteToPacket);
				apply(processEntry27andWriteToPacket);
				apply(processEntry28andWriteToPacket);
				apply(processEntry29andWriteToPacket);
				apply(processEntry30andWriteToPacket);
				apply(processEntry31andWriteToPacket);
				apply(processEntry32andWriteToPacket);
				/* Multicast Back */
	//		    apply(set_ack_table);
				apply(multicast_table);

	    	} else {
				apply(processEntry1);
				apply(processEntry2);
				apply(processEntry3);
				apply(processEntry4);
				apply(processEntry5);
				apply(processEntry6);
				apply(processEntry7);
				apply(processEntry8);
				apply(processEntry9);
				apply(processEntry10);
				apply(processEntry11);
				apply(processEntry12);
				apply(processEntry13);
				apply(processEntry14);
				apply(processEntry15);
				apply(processEntry16);
				apply(processEntry17);
				apply(processEntry18);
				apply(processEntry19);
				apply(processEntry20);
				apply(processEntry21);
				apply(processEntry22);
				apply(processEntry23);
				apply(processEntry24);
				apply(processEntry25);
				apply(processEntry26);
				apply(processEntry27);
				apply(processEntry28);
				apply(processEntry29);
				apply(processEntry30);
				apply(processEntry31);
				apply(processEntry32);
				apply(drop_table);
			}
		} else {//end of not seen this packet
			apply(read_counter);
			
			if(mdata.bitmap == 0){
				apply(Entry1WriteToPacket);
				apply(Entry2WriteToPacket);
				apply(Entry3WriteToPacket);
				apply(Entry4WriteToPacket);
				apply(Entry5WriteToPacket);
				apply(Entry6WriteToPacket);
				apply(Entry7WriteToPacket);
				apply(Entry8WriteToPacket);
				apply(Entry9WriteToPacket);
				apply(Entry10WriteToPacket);
				apply(Entry11WriteToPacket);
				apply(Entry12WriteToPacket);
				apply(Entry13WriteToPacket);
				apply(Entry14WriteToPacket);
				apply(Entry15WriteToPacket);
				apply(Entry16WriteToPacket);
				apply(Entry17WriteToPacket);
				apply(Entry18WriteToPacket);
				apply(Entry19WriteToPacket);
				apply(Entry20WriteToPacket);
				apply(Entry21WriteToPacket);
				apply(Entry22WriteToPacket);
				apply(Entry23WriteToPacket);
				apply(Entry24WriteToPacket);
				apply(Entry25WriteToPacket);
				apply(Entry26WriteToPacket);
				apply(Entry27WriteToPacket);
				apply(Entry28WriteToPacket);
				apply(Entry29WriteToPacket);
				apply(Entry30WriteToPacket);
				apply(Entry31WriteToPacket);
				apply(Entry32WriteToPacket);
				
				apply(echo_back_table);
			}	
		}    
	} else {
		apply(forward);
	}
}

control egress 
{
}

