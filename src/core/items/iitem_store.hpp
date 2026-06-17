#pragma once

#include "item.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

enum class StoreEvent : uint8_t { Added, Removed, Updated, Cleared };

using StoreObserver = std::function<void(StoreEvent, std::string_view uuid)>;

class IItemStore {
public:
    virtual ~IItemStore() = default;

    virtual std::string           add(Item item) = 0;
    virtual std::optional<Item>   get(std::string_view uuid) const = 0;
    virtual std::vector<Item>     getAll() const = 0;
    virtual bool                  update(std::string_view uuid, Item item) = 0;
    virtual bool                  remove(std::string_view uuid) = 0;
    virtual void                  clear() = 0;
    [[nodiscard]] virtual size_t  count() const noexcept = 0;

    virtual uint64_t observe(StoreObserver observer) = 0;
    virtual void     unobserve(uint64_t token) = 0;
};

} // namespace dd
