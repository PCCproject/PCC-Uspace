#include "CongestionEvents.h"
#include <unistd.h>

void OnPacketSent(PccSender& pcc_sender, uint64_t time, int32_t seq_no, int32_t packet_size) {
    pcc_sender.OnPacketSent(time, seq_no, packet_size);

    //sleep(1);

    /*
    std::cout << "Sent packet " << seq_no << std::endl;
    std::cout << "\tsize = " << packet_size << std::endl;
    std::cout << "\ttime = " << time << std::endl;
    */
}

void OnCongestionEvent(PccSender& pcc_sender, uint64_t time, uint64_t rtt, CongestionVector& acked_packets, CongestionVector& lost_packets) {
    pcc_sender.OnCongestionEvent(time, rtt, acked_packets, lost_packets);

    /*
    for (CongestionVector::iterator it = acked_packets.begin(); it != acked_packets.end(); ++it) {
        CongestionEvent ack_event = *it;
        std::cout << "Acked packet " << ack_event.seq_no << std::endl;
        std::cout << "\tsize = " << ack_event.acked_bytes << std::endl;
        std::cout << "\ttime = " << ack_event.time << std::endl;
    }
    */
}
