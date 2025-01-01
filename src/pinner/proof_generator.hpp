#ifndef RXREVOLTCHAIN_PROOF_GENERATOR_HPP
#define RXREVOLTCHAIN_PROOF_GENERATOR_HPP

#include <vector>
#include <string>
#include <random>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <ctime>
#include <mutex>

namespace rxrevoltchain {
namespace pinner {

/*
  proof_generator.hpp
  --------------------------------
  Creates ephemeral challenges (chunk requests) for proof-of-pinning.
  Helps verify node responses before passing them on to PoPConsensus.

  Required Methods (from specification):
    ProofGenerator() (default constructor)
    std::vector<size_t> GenerateRandomOffsets(size_t fileSize, size_t count)
    std::vector<uint8_t> ExtractChunks(const std::string &filePath, const std::vector<size_t> &offsets, size_t chunkSize)
    bool CompareChunks(const std::vector<uint8_t> &expected, const std::vector<uint8_t> &provided)

  "Fully functional" approach:
   - GenerateRandomOffsets: returns 'count' random positions in [0..fileSize-1] (or up to fileSize - chunkSize, depending on usage).
   - ExtractChunks: for each offset, reads 'chunkSize' bytes from 'filePath' (or less if near EOF).
   - CompareChunks: basic equality check between two byte vectors.

   Notes:
   - For large files, you may want more advanced chunking or streaming logic.
   - This demonstration uses std::mt19937 for random generation.
   - Make sure to seed or handle concurrency as needed if random offsets are called frequently.
*/

class ProofGenerator
{
public:
    // Default constructor
    ProofGenerator()
    {
        // Seed the random generator with a device-based seed
        std::random_device rd;
        m_rng.seed(rd());
    }

    /*
      GenerateRandomOffsets
      --------------------------------
      - Returns 'count' random positions within [0..fileSize-1].
      - If you want chunk-based logic, you could ensure offsets align to chunk boundaries
        or reduce the upper bound (e.g., fileSize - chunkSize).
      - For demonstration, we simply pick random positions in [0..fileSize-1].
    */
    std::vector<size_t> GenerateRandomOffsets(size_t fileSize, size_t count)
    {
        if (fileSize == 0 || count == 0)
        {
            return {};
        }

        std::uniform_int_distribution<size_t> dist(0, fileSize - 1);

        std::vector<size_t> offsets;
        offsets.reserve(count);
        for (size_t i = 0; i < count; i++)
        {
            offsets.push_back(dist(m_rng));
        }
        return offsets;
    }

    /*
      ExtractChunks
      --------------------------------
      - For each offset in 'offsets', reads 'chunkSize' bytes (or until EOF) from the file.
      - Concatenates all extracted data in a single buffer (some PoP schemes might store them separately).
      - Returns the combined buffer.

      If you want them separate, you could store each chunk in a structure or return a vector<vector<uint8_t>>.
      But the spec says a single std::vector<uint8_t>, so we’ll just append them.
    */
    std::vector<uint8_t> ExtractChunks(const std::string &filePath,
                                       const std::vector<size_t> &offsets,
                                       size_t chunkSize)
    {
        std::vector<uint8_t> allChunks;
        if (chunkSize == 0 || offsets.empty())
        {
            return allChunks; // empty
        }

        std::ifstream ifs(filePath, std::ios::binary);
        if (!ifs.is_open())
        {
            throw std::runtime_error("ProofGenerator::ExtractChunks - Failed to open file: " + filePath);
        }

        for (auto off : offsets)
        {
            // Seek to 'off'
            ifs.seekg(static_cast<std::streamoff>(off), std::ios::beg);
            if (!ifs.good())
            {
                // Seek may fail if 'off' is beyond EOF
                continue;
            }

            // Read chunkSize bytes (or less if near EOF)
            std::vector<uint8_t> chunk(chunkSize, 0);
            ifs.read(reinterpret_cast<char*>(chunk.data()), chunkSize);
            size_t bytesRead = static_cast<size_t>(ifs.gcount());
            chunk.resize(bytesRead);

            // Append to allChunks
            allChunks.insert(allChunks.end(), chunk.begin(), chunk.end());
        }

        return allChunks;
    }

    /*
      CompareChunks
      --------------------------------
      - Basic equality check between two byte buffers.
      - Returns true if they match exactly (same size, same contents).
    */
    bool CompareChunks(const std::vector<uint8_t> &expected,
                       const std::vector<uint8_t> &provided)
    {
        if (expected.size() != provided.size())
        {
            return false;
        }
        return std::equal(expected.begin(), expected.end(), provided.begin());
    }

private:
    // We keep a random engine for generating offsets
    std::mt19937_64 m_rng;
};

} // namespace pinner
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_PROOF_GENERATOR_HPP
