//***************************************************************************
//* Copyright (c) 2015 Saint Petersburg State University
//* Copyright (c) 2011-2014 Saint Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#pragma once

#include "utils/ph_map/perfect_hash_map.hpp"
#include "utils/kmer_mph/kmer_index.hpp"
#include <array>

namespace debruijn_graph {

class LinkCleaner {
private:
    typedef utils::DeBruijnExtensionIndex<> Index;
    typedef Index::KMer Kmer;
    typedef Index::KeyWithHash KeyWithHash;
    Index &index_;

    void CleanForwardLinks(const KeyWithHash &kh, char ch) {
        if (index_.CheckOutgoing(kh, ch)) {
            KeyWithHash next_kh = index_.GetOutgoing(kh, ch);
            if (!index_.CheckIncoming(next_kh, kh[0])) {
                index_.DeleteOutgoing(kh, ch);
            }
        }
    }

    void CleanBackwardLinks(const KeyWithHash &kh, char ch) {
        if (index_.CheckIncoming(kh, ch)) {
            KeyWithHash prev_kh = index_.GetIncoming(kh, ch);
            if (!index_.CheckOutgoing(prev_kh, kh[index_.k() - 1])) {
                index_.DeleteIncoming(kh, ch);
            }
        }
    }

public:
    LinkCleaner(Index &index) : index_(index) {}

    //TODO make parallel
    void CleanLinks() {
        vector<Index::kmer_iterator> iters = index_.kmer_begin(10 * omp_get_max_threads());
#pragma omp parallel for schedule(guided)
        for (size_t i = 0; i < iters.size(); i++) {
            for (Index::kmer_iterator &it = iters[i]; it.good(); ++it) {
                const KeyWithHash kh = index_.ConstructKWH(RtSeq(index_.k(), *it));
                if (kh.is_minimal()) {
                    for (char ch = 0; ch < 4; ch++) {
                        CleanForwardLinks(kh, ch);
                        CleanBackwardLinks(kh, ch);
                    }
                }
            }
        }
    }
};

class AlternativeEarlyTipClipper {
public:
    typedef utils::DeBruijnExtensionIndex<> Index;
    typedef Index::KMer Kmer;
    typedef Index::KeyWithHash KeyWithHash;

    AlternativeEarlyTipClipper(Index &index, size_t length_bound) : index_(index), length_bound_(length_bound) {}

    /*
     * Method returns the number of removed edges
     */
    size_t ClipTips() {
        INFO("Early tip clipping");
        size_t result = RoughClipTips(10 * omp_get_max_threads());
        CleanLinks();
        INFO(result << " " << (index_.k() + 1) << "-mers were removed by early tip clipper");
        return result;
    }

    size_t RoughClipTips(size_t n_chunks, const std::vector<size_t> &chunks) {
        std::vector<Index::kmer_iterator> all_iters = index_.kmer_begin(n_chunks);
        std::vector<Index::kmer_iterator> iters;
        for (size_t chunk : chunks) {
            if (chunk < all_iters.size()) {  // all_iters.size() could be less than required
                iters.push_back(std::move(all_iters[chunk]));
            }
        }

        return RoughClipTips(iters);
    }

    void CleanLinks() {
        // Do nothing
        // LinkCleaner(index_).CleanLinks();
    }

private:
    Index &index_;
    size_t length_bound_;

    /*
     * This method starts from the kmer that is second in the tip counting from junction vertex. It records all kmers of a tip into tip vector.
     * The method returns length of a tip.
     * In case it did not end as a tip or if it was too long tip vector is cleared and infinite length is returned.
     * Thus tip vector contains only kmers to be removed while returned length value gives reasonable information of what happend.
     */
    size_t FindForward(KeyWithHash kh, vector<KeyWithHash> &tip) {
        while (tip.size() < length_bound_ && index_.CheckUniqueIncoming(kh) && index_.CheckUniqueOutgoing(kh)) {
            tip.push_back(kh);
            kh = index_.GetUniqueOutgoing(kh);
        }
        tip.push_back(kh);
        if (index_.CheckUniqueIncoming(kh) && index_.IsDeadEnd(kh)) {
            return tip.size();
        }
        tip.clear();
        return -1;
    }

    size_t FindBackward(KeyWithHash kh, vector<KeyWithHash> &tip) {
        while (tip.size() < length_bound_ && index_.CheckUniqueOutgoing(kh) && index_.CheckUniqueIncoming(kh)) {
            tip.push_back(kh);
            kh = index_.GetUniqueIncoming(kh);
        }
        tip.push_back(kh);
        if (index_.CheckUniqueOutgoing(kh) && index_.IsDeadStart(kh)) {
            return tip.size();
        }
        tip.clear();
        return -1;
    }

    size_t RemoveTip(const vector<KeyWithHash> &tip) {
        for (const auto &kh : tip) index_.IsolateVertex(kh);
        return tip.size();
    }

    size_t RemoveTips(const std::array<vector<KeyWithHash>, 4> &tips, size_t max) {
        size_t result = 0;
        for (const auto &tip : tips) {
            if (tip.size() < max) result += RemoveTip(tip);
        }
        return result;
    }

    size_t RemoveForward(KeyWithHash kh) {
        std::array<vector<KeyWithHash>, 4> tips;
        size_t max = 0;
        for (char c = 0; c < 4; c++) {
            if (index_.CheckOutgoing(kh, c)) {
                KeyWithHash khc = index_.GetOutgoing(kh, c);
                size_t len = FindForward(khc, tips[c]);
                if (len > max) max = len;
            }
        }
        return RemoveTips(tips, max);
    }

    size_t RemoveBackward(KeyWithHash kh) {
        std::array<vector<KeyWithHash>, 4> tips;
        size_t max = 0;
        for (char c = 0; c < 4; c++) {
            if (index_.CheckIncoming(kh, c)) {
                KeyWithHash khc = index_.GetIncoming(kh, c);
                size_t len = FindBackward(khc, tips[c]);
                if (len > max) max = len;
            }
        }
        return RemoveTips(tips, max);
    }

    size_t RoughClipTips(std::vector<Index::kmer_iterator> &iters) {
        std::vector<size_t> result(iters.size());
        std::vector<std::vector<KeyWithHash>> tipped_junctions(iters.size());
#pragma omp parallel for schedule(guided)
        for (size_t i = 0; i < iters.size(); i++) {
            for (Index::kmer_iterator &it = iters[i]; it.good(); ++it) {
                KeyWithHash kh = index_.ConstructKWH(RtSeq(index_.k(), *it));
                if (index_.OutgoingEdgeCount(kh) >= 2) {
                    size_t removed = RemoveForward(kh);
                    result[i] += removed;
                    if (removed) {
                        tipped_junctions[i].push_back(kh);
                    }
                }
            }
        }
        size_t sum = 0;
        for (size_t i = 0; i < result.size(); i++) sum += result[i];

        // Remove links leading to tips
        size_t clipped_tips = 0;
#pragma omp parallel for schedule(guided) reduction(+:clipped_tips)
        for (size_t i = 0; i < tipped_junctions.size(); i++) {
            for (const KeyWithHash &kh : tipped_junctions[i]) {
                clipped_tips += CleanForwardLinks(kh);
            }
        }

        return sum;
    }

    size_t RoughClipTips(size_t n_chunks) {
        auto iters = index_.kmer_begin(n_chunks);
        return RoughClipTips(iters);
    }

    bool CleanForwardLinks(const KeyWithHash &kh, char ch) {
        if (index_.CheckOutgoing(kh, ch)) {
            KeyWithHash next_kh = index_.GetOutgoing(kh, ch);
            if (!index_.CheckIncoming(next_kh, kh[0])) {
                index_.DeleteOutgoing(kh, ch);
                return true;
            }
        }
        return false;
    }

    size_t CleanForwardLinks(const KeyWithHash &kh) {
        size_t count = 0;
        for (char ch = 0; ch < 4; ++ch) {
            count += CleanForwardLinks(kh, ch);
        }
        return count;
    }

protected:
    DECL_LOGGER("Early tip clipping");
};

}  // namespace debruijn_graph
