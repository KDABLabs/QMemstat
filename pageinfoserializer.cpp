/*
 serialized format:
    uint64_t length (in bytes, length field not included in length)
    repeat
        // note this is one MappedRegion entry
        uint64_t MappedRegion::start
        uint64_t MappedRegion::end
        uint32_t useCounts[(MappedRegion::end - MappedRegion::start) / PageInfo::pageSize]
        uint32_t combinedFlags[(MappedRegion::end - MappedRegion::start) / PageInfo::pageSize]
    until read position == length + sizeof(length)
    ... at exactly which point the last MappedRegion must also end, obviously

  there is no endianness flag - little endian is used because it's the only endianness of x86 and
  the default endianness on ARM
 */

class PageInfoSerializer
{
public:
    PageInfoSerializer(const PageInfo &pageInfo)
       : m_mappedRegions(pageInfo.mappedRegions()),
         m_region(-1),
         m_posInRegion(0)
    {}
    pair<const char*, size_t> serializeMore();

private:
    bool nextRegionIf(bool cond);
    template<typename T>
    bool placePrimitiveTypeAt(T value, size_t *bufPos, size_t *staticMemberOffset = nullptr);
    static size_t chunkSize() { return sizeof(m_buffer); }

    const std::vector<MappedRegion> &m_mappedRegions;
    int m_region;
    size_t m_posInRegion;
    char m_buffer[16 * 1024]; // set size to 9 or 11 or so (must be >= sizeof(uint64_t) for stress testing
};

template<typename T>
bool PageInfoSerializer::placePrimitiveTypeAt(T value, size_t *bufPos, size_t *staticMemberOffset)
{
    bool ok = true;
    if (staticMemberOffset) {
        // is this the right "struct" position to put the value?
        ok = ok && *staticMemberOffset == m_posInRegion;
        // always update *staticMemberOffset!! - it's used for *static* position bookkeeping
        *staticMemberOffset += sizeof(value);
    }
    ok = ok && *bufPos + sizeof(value) <= chunkSize();
    if (ok) {
        *reinterpret_cast<T *>(m_buffer + *bufPos) = value;
        *bufPos += sizeof(value);
        m_posInRegion += sizeof(value);
    }
    return true;
}

bool PageInfoSerializer::nextRegionIf(bool cond)
{
    if (cond) {
        m_posInRegion = 0;
        m_region++;
    }
    return cond;
}

pair<const char*, size_t> PageInfoSerializer::serializeMore()
{
    size_t bufPos = 0;
    if (m_region == -1) {
        // write the size of the whole list of MappedRegions into the output as a framing header
        uint64_t size = m_mappedRegions.size() * 2 * sizeof(uint64_t); // all the "start" and "end" members
        // useCounts and combinedFlags data
        for (const MappedRegion &mr : m_mappedRegions) {
            size += (mr.end - mr.start) / PageInfo::pageSize * 2 * sizeof(uint32_t);
        }
        placePrimitiveTypeAt(size, &bufPos);
        nextRegionIf(true);
    }

    // concept: use byte position in region (m_posInRegion) to find which member we're at, write the member,
    //          repeat until end of data or end of buffer

    // note that we need to stop if we went through an iteration that didn't find enough room for the current
    // member to write, but also did not fill the buffer completely
    bool wrote = true;
    while (wrote && m_region < m_mappedRegions.size() && bufPos < chunkSize()) {
        wrote = false;
        const MappedRegion &mr = m_mappedRegions[m_region];

        size_t regionMemberOffset = 0;

        // || wrote last to prevent short-circuit evaluation eliminating the call doing the actual write!
        wrote = placePrimitiveTypeAt(mr.start, &bufPos, &regionMemberOffset) || wrote;
        wrote = placePrimitiveTypeAt(mr.end, &bufPos, &regionMemberOffset) || wrote;

        if (m_posInRegion >= regionMemberOffset) {
            const size_t arraySize = (mr.end - mr.start) / PageInfo::pageSize * sizeof(uint32_t);
            if (nextRegionIf(!arraySize)) {
                wrote = true;
                continue;
            }

            const bool isFlags = m_posInRegion >= regionMemberOffset + arraySize;
            if (isFlags) {
                regionMemberOffset += arraySize;
            }
            const size_t arrayEnd = regionMemberOffset + arraySize;
            assert(m_posInRegion < arrayEnd && m_posInRegion < 2 * (sizeof(uint64_t) + arraySize));

            const char *const data = reinterpret_cast<const char*>(isFlags ? &mr.combinedFlags[0]
                                                                           : &mr.useCounts[0])
                                         + m_posInRegion - regionMemberOffset;

            if (bufPos == 0 && m_posInRegion + chunkSize() <= arrayEnd) {
                // zero-copy fast path!
                m_posInRegion += chunkSize();
                nextRegionIf(isFlags && m_posInRegion >= arrayEnd);
                // it may or may not be a a good idea to send more than the usual chunkSize() in this case;
                // it may not be a good idea because large write()s might use more buffer memory somewhere
                return make_pair(data, chunkSize());
            }

            size_t amount = min(chunkSize() - bufPos, arrayEnd - m_posInRegion);
            memcpy(m_buffer + bufPos, data, amount);
            m_posInRegion += amount;
            bufPos += amount;
            wrote = true;

            // if we just finished copying useCounts, we'll copy combinedFlags in the next while loop
            // iteration using (and thus exercising) the resumable serialization that we do anyway.

            assert(m_posInRegion <= arrayEnd && m_posInRegion <= 2 * (sizeof(uint64_t) + arraySize));
            nextRegionIf(isFlags && m_posInRegion >= arrayEnd);
        }
    }

    return make_pair(m_buffer, bufPos);
}
