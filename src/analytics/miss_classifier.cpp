#include "ccsim/analytics/miss_classifier.hpp"

#include <algorithm>
#include <stdexcept>

namespace ccsim {

FullyAssociativeLru::FullyAssociativeLru(std::uint64_t capacity_lines)
    : capacity_(static_cast<std::uint32_t>(capacity_lines)),
      where_(static_cast<std::size_t>(std::min<std::uint64_t>(capacity_lines, 1U << 20))) {
    if (capacity_lines == 0 || capacity_lines >= kNil) {
        throw std::invalid_argument("FullyAssociativeLru: capacity out of range");
    }
}

void FullyAssociativeLru::unlink(std::uint32_t n) noexcept {
    const std::uint32_t p = prev_[n], x = next_[n];
    if (p != kNil) next_[p] = x; else head_ = x;
    if (x != kNil) prev_[x] = p; else tail_ = p;
}

void FullyAssociativeLru::push_front(std::uint32_t n) noexcept {
    prev_[n] = kNil;
    next_[n] = head_;
    if (head_ != kNil) prev_[head_] = n;
    head_ = n;
    if (tail_ == kNil) tail_ = n;
}

bool FullyAssociativeLru::access(std::uint64_t line) {
    const std::uint32_t n = where_.find(line);
    if (n != LineIndex::kAbsent) {
        if (n != head_) {
            unlink(n);
            push_front(n);
        }
        return true;
    }
    std::uint32_t slot;
    if (used_ < capacity_) {
        slot = used_++;  // grow lazily: a huge shadow costs nothing until it fills
        line_.push_back(0);
        prev_.push_back(kNil);
        next_.push_back(kNil);
    } else {
        slot = tail_;
        unlink(slot);
        where_.erase(line_[slot]);
    }
    line_[slot] = line;
    where_.put(line, slot);
    push_front(slot);
    return false;
}

std::size_t FullyAssociativeLru::memory_bytes() const noexcept {
    return line_.capacity() * 8 + (prev_.capacity() + next_.capacity()) * 4 + where_.memory_bytes();
}

void MissClassifier::observe(std::uint64_t line, bool real_hit) {
    const bool shadow_hit = shadow_.access(line);
    const bool first_touch = seen_.find(line) == LineIndex::kAbsent;
    if (first_touch) seen_.put(line, 0);
    if (real_hit) {
        if (!shadow_hit) ++counts_.shadow_only_misses;
        return;
    }
    if (first_touch) ++counts_.compulsory;
    else if (shadow_hit) ++counts_.conflict;
    else ++counts_.capacity;
}

}  // namespace ccsim
