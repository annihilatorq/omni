#ifndef SHADOW_MODULE_VIEW_HPP
#define SHADOW_MODULE_VIEW_HPP

#include "dynamic_link_library.hpp"
#include "win/peb.hpp"
#include <iterator>

namespace shadow {
    class module_view {
    public:
        explicit module_view() {
            auto entry = &win::PEB::loader_data()->in_load_order_module_list;
            m_begin = entry->flink;
            m_end = entry;
        }

        module_view& skip_module() {
            m_begin = m_begin->flink;
            return *this;
        }

        class iterator {
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = dynamic_link_library;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            iterator() noexcept( std::is_nothrow_default_constructible_v<value_type> ): m_entry( nullptr ), m_value( {} ) { }
            ~iterator() = default;
            iterator( const iterator& ) = default;
            iterator( iterator&& ) = default;
            iterator& operator=( iterator&& ) = default;
            iterator( win::list_entry* entry ): m_entry( entry ) {
                on_update();
            }

            pointer operator->() const noexcept {
                return &m_value;
            }

            iterator& operator=( const iterator& other ) noexcept {
                if ( this != &other ) {
                    m_entry = other.m_entry;
                    on_update();
                }
                return *this;
            }

            iterator& operator++() noexcept {
                m_entry = m_entry->flink;
                on_update();
                return *this;
            }

            iterator operator++( int ) noexcept {
                iterator temp = *this;
                ++( *this );
                return temp;
            }

            iterator& operator--() noexcept {
                m_entry = m_entry->blink;
                on_update();
                return *this;
            }

            iterator operator--( int ) noexcept {
                iterator temp = *this;
                --( *this );
                return temp;
            }

            bool operator==( const iterator& other ) const noexcept {
                return m_entry == other.m_entry;
            }

            bool operator!=( const iterator& other ) const noexcept {
                return !( *this == other );
            }

            reference operator*() const noexcept {
                return m_value;
            }

        private:
            void on_update() const noexcept {
                auto table_entry = win::containing_record( m_entry, &win::loader_table_entry::in_load_order_links );
                m_value = dynamic_link_library{ table_entry };
            }

            win::list_entry* m_entry;
            mutable value_type m_value;
        };

        // Make sure the iterator is compatible with std::ranges
        static_assert( std::bidirectional_iterator<iterator> );

        iterator begin() const noexcept {
            return iterator( m_begin );
        }
        iterator end() const noexcept {
            return iterator( m_end );
        }

        // \brief Find an export with user-defined predicate
        // \param predicate User-defined predicate
        // \return Iterator pointing to [name, address] if
        // export is found, .end() if export is not found.
        [[nodiscard]] iterator find_if( std::predicate<iterator::value_type> auto predicate ) const noexcept {
            return std::ranges::find_if( *this, predicate );
        }

    private:
        win::list_entry* m_begin{ nullptr };
        win::list_entry* m_end{ nullptr };
    };
} // namespace shadow

#endif // SHADOW_MODULE_VIEW_HPP
