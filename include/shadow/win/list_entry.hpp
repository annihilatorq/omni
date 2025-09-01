#ifndef SHADOW_WIN_LIST_ENTRY_HPP
#define SHADOW_WIN_LIST_ENTRY_HPP

namespace win {
    struct list_entry {
        list_entry* flink;
        list_entry* blink;
    };
} // namespace win

#endif // SHADOW_WIN_LIST_ENTRY_HPP
