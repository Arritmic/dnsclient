#include "MXResourceRecord.h"

#include <arpa/inet.h>

namespace ro1617 {

    MXResourceRecord::MXResourceRecord(const DomainName& domain,
                                       const uint8_t *rdata_cbegin, const uint8_t *rdata_cend,
                                       uint32_t ttl, uint16_t rdlength, const uint8_t *cache) :
        NSResourceRecord(domain, rdata_cbegin+2, rdata_cend, ttl, rdlength-2u, cache)
        //                       ^^^ Domain name is after the 16-bits preference integer
    {
        memcpy(&preference_, rdata_cbegin, 2);
        preference_ = ntohs(preference_);
        // No need to check for rdlength coherence. Done by our parent class.
    }

}
