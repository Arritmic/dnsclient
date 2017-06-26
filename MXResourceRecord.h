#ifndef MXRESOURCERECORD_H
#define MXRESOURCERECORD_H

#include "NSResourceRecord.h"

namespace ro1617
{

    /**
     * @brief A class for serialization/deseralization of an MX RR record, as per section 3.3.9 of RFC 1035
     *
     * Identical to the NSResourceRecord with the addition of the 16-bits preference value
     */
    class MXResourceRecord : public NSResourceRecord
    {
    public:
        /**
         * @brief Builds an MX RR from a partially decoded serialization of an RR
         */
        MXResourceRecord(const DomainName& domain,
                         const uint8_t *rdata_cbegin, const uint8_t *rdata_cend,
                         uint32_t ttl, uint16_t rdlength, const uint8_t *cache);

    private:
        std::uint16_t preference_;
    };
}

#endif // MXRESOURCERECORD_H
