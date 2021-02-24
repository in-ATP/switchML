#define MAX_ENTRIES_PER_PACKET 32
/*************************************************************************
 ***********************  H E A D E R S  *********************************
 *************************************************************************/
header_type ethernet_t {
    fields {
        dstAddr   : 48;
        srcAddr   : 48;
        etherType : 16;
    }
}

header_type ipv4_t {
    fields {
        version        : 4;
        ihl            : 4;
        dscp           : 6;
        ecn            : 2;
        totalLen       : 16;
        identification : 16;
        flags          : 3;
        fragOffset     : 13;
        ttl            : 8;
        protocol       : 8;
        hdrChecksum    : 16;
        srcAddr        : 32;
        dstAddr        : 32;
    }
}

header_type udp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        length_ : 16;
        checksum : 16;
    }
}

header_type p4ml_t {
    fields {
        key            :  64;
        len_tensor     :  32;
        bitmap         :  32;
	    agtr_time      :  8;
	    reserved       :  1;
        isFirstTime    :  1;
        version        :  1;
        dataIndex      :  1;
        ECN            :  1;
        isResend       :  1;
        isTerminated   :  1;
        isACK          :  1;
        agtr           :  16;
        appIDandSeqNum :  32;
        versionIndex   :  32;  //in switchml.p4: this is used to find the bit location 
    }
}

header_type entry_t {
    fields {
        data0        : 32 (signed);
        data1        : 32 (signed);
        data2        : 32 (signed);
        data3        : 32 (signed);
        data4        : 32 (signed);
        data5        : 32 (signed);
        data6        : 32 (signed);
        data7        : 32 (signed);
        data8        : 32 (signed);
        data9        : 32 (signed);
        data10        : 32 (signed);
        data11        : 32 (signed);
        data12        : 32 (signed);
        data13        : 32 (signed);
        data14        : 32 (signed);
        data15        : 32 (signed);
        data16        : 32 (signed);
        data17        : 32 (signed);
        data18        : 32 (signed);
        data19        : 32 (signed);
        data20        : 32 (signed);
        data21        : 32 (signed);
        data22        : 32 (signed);
        data23        : 32 (signed);
        data24        : 32 (signed);
        data25        : 32 (signed);
        data26        : 32 (signed);
        data27        : 32 (signed);
        data28        : 32 (signed);
        data29        : 32 (signed);
        data30        : 32 (signed);
        data31        : 32 (signed);
    }
}

// header_type entry_t {
//     fields {
//         data        : 32 (signed);
//     }
// }

/*************************************************************************
 ***********************  M E T A D A T A  *******************************
 *************************************************************************/

header_type p4ml_meta_t {
    fields {
        ack                      : 1;
        isResend                 : 32;
        isMyAppIDandMyCurrentSeq : 32;
        bitmap                   : 32;
        current_counter          : 8;
        isAggregate              : 8;
        qdepth                   : 32;
        position                 : 8;
        isDrop                   : 32;
	}
}
