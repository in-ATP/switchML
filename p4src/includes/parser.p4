

metadata p4ml_meta_t mdata;

header ethernet_t ethernet;
header ipv4_t     ipv4;
header udp_t      udp;
header p4ml_t     p4ml;
header p4ml_t     p4ml_bg;

// header entry_t    entry[MAX_ENTRIES_PER_PACKET];
header entry_t    p4ml_entries;

/*************************************************************************
 ***********************  P A R S E R  ***********************************
 *************************************************************************/

parser start {
    extract(ethernet);
    return select(ethernet.etherType) {
        0x0700   : parse_ipv4;
        0x0800   : parse_rdma;
        0x0900   : parse_bg;
        default  : ingress;
    }
}

parser parse_ipv4 {
    extract(ipv4);
    return parse_p4ml;
}

parser parse_udp {
    extract(udp);
    return parse_p4ml;
}

parser parse_p4ml {
    extract(p4ml);
    set_metadata(mdata.qdepth, 0);    
//    return ingress; 
   return parse_entry;
}

parser parse_entry {
	extract(p4ml_entries);
	return ingress;
}

/* RDMA */
parser parse_rdma {
    extract(ipv4);
    return ingress;
}

/* BG */
parser parse_bg {
    extract(ipv4);
    return parse_udp_bg;
}

parser parse_udp_bg {
    extract(udp);
    return parse_p4ml_bg;
}

parser parse_p4ml_bg {
    extract(p4ml_bg);
    set_metadata(mdata.qdepth, 0);    
//    return ingress; 
   return ingress;
}
