//     Copyright Toru Niina 2018.
// Distributed under the MIT License.
#ifndef PNM_PLUS_PLUS_H
#define PNM_PLUS_PLUS_H

#if !defined(__cplusplus)
#error "pnm++ is a library for C++."
#endif

#if __cplusplus < 201103L
#error "pnm++ requires C++11 or later."
#endif

#include <type_traits>
#include <utility>
#include <stdexcept>
#include <iterator>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <ostream>
#include <fstream>
#include <sstream>
#include <cstdint>

namespace pnm
{

// --------------------------------------------------------------------------
//                               * basic_pixel
//          _            _         - basic_pixel<T, 1>
//   _ __  (_)__  _ ___ | | ___    - basic_pixel<T, 3>
//  | '_ \ | |\ \/ / _ \| |/ __| * aliases
//  | |_) )| | )  (  __/| |\__ \   - bit_pixel for bitmap image
//  | .__/ |_|/_/\_\___||_||___/   - gray_pixel for grayscale image
//  |_|                            - pix_pixel for RGB color image
//                               * literals
// --------------------------------------------------------------------------
template<typename T, std::size_t N>
struct basic_pixel;

template<typename T>
struct basic_pixel<T, 1>
{
  public:
    using value_type = T;
    static constexpr std::size_t colors = 1;

    basic_pixel()  = default;
    ~basic_pixel() = default;
    basic_pixel(const basic_pixel&) = default;
    basic_pixel(basic_pixel&&)      = default;
    basic_pixel& operator=(const basic_pixel&) = default;
    basic_pixel& operator=(basic_pixel&&)      = default;

    basic_pixel(const value_type& v)
        noexcept(std::is_nothrow_copy_constructible<value_type>::value)
        : value(v)
    {}
    basic_pixel(value_type&& v)
        noexcept(std::is_nothrow_move_constructible<value_type>::value)
        : value(std::move(v))
    {}

    value_type value;
};
template<typename T>
constexpr std::size_t basic_pixel<T, 1>::colors;

template<typename T>
struct basic_pixel<T, 3>
{
  public:
    using value_type = T;
    static constexpr std::size_t colors = 3;

    basic_pixel()  = default;
    ~basic_pixel() = default;
    basic_pixel(const basic_pixel&) = default;
    basic_pixel(basic_pixel&&)      = default;
    basic_pixel& operator=(const basic_pixel&) = default;
    basic_pixel& operator=(basic_pixel&&)      = default;

    basic_pixel(const value_type& R, const value_type& G, const value_type& B)
        noexcept(std::is_nothrow_copy_constructible<value_type>::value)
        : red(R), green(G), blue(B)
    {}
    basic_pixel(value_type&& R, value_type&& G, value_type&& B)
        noexcept(std::is_nothrow_move_constructible<value_type>::value)
        : red(std::move(R)), green(std::move(G)), blue(std::move(B))
    {}

    value_type red;
    value_type green;
    value_type blue;
};
template<typename T>
constexpr std::size_t basic_pixel<T, 3>::colors;

using    bit_pixel = basic_pixel<bool,          1>;
using   gray_pixel = basic_pixel<std::uint8_t,  1>;
using    rgb_pixel = basic_pixel<std::uint8_t,  3>;
// XXX not supported yet
// using gray16_pixel = basic_pixel<std::uint16_t, 1>;
// using  rgb16_pixel = basic_pixel<std::uint16_t, 3>;

namespace literals
{
inline namespace pixel_literals
{
inline bit_pixel operator"" _bit(unsigned long long x)
{
    return bit_pixel(x != 0);
}
inline gray_pixel operator"" _gray(unsigned long long x)
{
    return gray_pixel(static_cast<std::uint8_t>(x & 0xFFu));
}
inline rgb_pixel operator"" _rgb(unsigned long long x)
{
    const std::uint8_t R(static_cast<std::uint8_t>((x >> 16) & 0xFFu));
    const std::uint8_t G(static_cast<std::uint8_t>((x >>  8) & 0xFFu));
    const std::uint8_t B(static_cast<std::uint8_t>( x        & 0xFFu));
    return rgb_pixel(R, G, B);
}
} // pixel_literals
} // literals

// --------------------------------------------------------------------------
//  _ __  _ __ _____  ____  __ * detail::(const_)line_proxy
// | '_ \| '_// _ \ \/ /\ \/ /   to enable `image[y][x]`
// | |_) | | ( (_) )  (  \  /  * detail::(const_)line_iterator
// | .__/|_|  \___/_/\_\ / /     to enable `for(auto line : image.lines())`
// |_|                  /_/    * detail::range<iterator>
//                              just a pair of iterators.
// --------------------------------------------------------------------------
namespace detail
{

template<typename Container> struct line_proxy_iterator;
template<typename Container> struct const_line_proxy_iterator;
// XXX not supported yet
// template<typename Container> struct reverse_line_proxy_iterator;
// template<typename Container> struct const_reverse_line_proxy_iterator;

template<typename Container>
struct line_proxy
{
    using container_type  = Container;
    using reference       = typename container_type::reference;
    using const_reference = typename container_type::const_reference;
    using iterator        = typename container_type::iterator;
    using const_iterator  = typename container_type::const_iterator;

    line_proxy(container_type& c, std::size_t iy, std::size_t nx) noexcept
        : nx_(nx), iy_(iy), offset_(nx * iy), container_(std::addressof(c))
    {}
    ~line_proxy() = default;

    line_proxy(const line_proxy& other)
        : nx_(other.nx_), iy_(other.iy_), offset_(other.offset_),
          container_(other.container_)
    {}
    line_proxy& operator=(const line_proxy& other)
    {
        if(this->nx_ != other.nx_)
        {
            throw std::out_of_range("pnm::image::line_proxy::copy width(" +
                std::to_string(this->nx_) + std::string(") and (") +
                std::to_string(other.nx_) + std::string(") differs"));
        }
        for(std::size_t i=0; i<this->nx_; ++i)
        {
            (*this)[i] = other[i];
        }
        return *this;
    }

    std::size_t width()      const noexcept {return nx_;}
    std::size_t y_position() const noexcept {return iy_;}

    reference       operator[](const std::size_t i)       noexcept
    {return (*container_)[offset_ + i];}
    const_reference operator[](const std::size_t i) const noexcept
    {return (*container_)[offset_ + i];}

    reference at(const std::size_t i) noexcept
    {
        if(nx_ <= i)
        {
            throw std::out_of_range("pnm::image::line_proxy::at: index (" +
                std::to_string(i)   + std::string(") exceeds width(") +
                std::to_string(nx_) + std::string(")"));
        }
        return container_->at(offset_ + i);
    }
    const_reference at(const std::size_t i) const noexcept
    {
        if(nx_ <= i)
        {
            throw std::out_of_range("pnm::image::line_proxy::at: index (" +
                std::to_string(i)   + std::string(") exceeds width(") +
                std::to_string(nx_) + std::string(")"));
        }
        return container_->at(offset_ + i);
    }

    iterator       begin()        noexcept {return container_->begin() + offset_;}
    iterator       end()          noexcept {return container_->begin() + offset_ + nx_;}
    const_iterator begin()  const noexcept {return container_->begin() + offset_;}
    const_iterator end()    const noexcept {return container_->begin() + offset_ + nx_;}
    const_iterator cbegin() const noexcept {return container_->begin() + offset_;}
    const_iterator cend()   const noexcept {return container_->begin() + offset_ + nx_;}

    bool operator==(const line_proxy& rhs) const noexcept
    {
        return this->nx_        == rhs.nx_ &&
               this->iy_        == rhs.iy_ &&
               this->container_ == rhs.container_;
    }
    bool operator!=(const line_proxy& rhs) const noexcept
    {
        return !(*this == rhs);
    }
    bool operator<(const line_proxy& rhs) const noexcept
    {
        return this->nx_ == rhs.nx_ && this->container_ == rhs.container_ &&
               this->iy_ <  rhs.ny_;
    }
    bool operator>(const line_proxy& rhs) const noexcept
    {
        return this->nx_ == rhs.nx_ && this->container_ == rhs.container_ &&
               this->iy_ >  rhs.ny_;
    }
    bool operator<=(const line_proxy& rhs) const noexcept
    {
        return this->nx_ == rhs.nx_ && this->container_ == rhs.container_ &&
               this->iy_ <= rhs.ny_;
    }
    bool operator>=(const line_proxy& rhs) const noexcept
    {
        return this->nx_ == rhs.nx_ && this->container_ == rhs.container_ &&
               this->iy_ >= rhs.ny_;
    }

  private:

    friend struct line_proxy_iterator<container_type>;
//     friend struct reverse_line_proxy_iterator<container_type>;

    std::size_t     nx_, iy_;
    std::size_t     offset_;
    container_type* container_;
};

template<typename Container>
struct const_line_proxy
{
    using container_type  = Container;
    using reference       = typename container_type::reference;
    using const_reference = typename container_type::const_reference;
    using iterator        = typename container_type::iterator;
    using const_iterator  = typename container_type::const_iterator;

    const_line_proxy(const container_type& c,
                     std::size_t iy, std::size_t nx) noexcept
        : nx_(nx), iy_(iy), offset_(nx * iy), container_(std::addressof(c))
    {}
    ~const_line_proxy() = default;

    const_line_proxy(const line_proxy<container_type>& other)
        : nx_(other.nx_), iy_(other.iy_), offset_(other.offset_),
          container_(other.container_)
    {}
    const_line_proxy(const const_line_proxy& other)
        : nx_(other.nx_), iy_(other.iy_), offset_(other.offset_),
          container_(other.container_)
    {}

    std::size_t width()      const noexcept {return nx_;}
    std::size_t y_position() const noexcept {return iy_;}

    const_reference operator[](const std::size_t i) const noexcept
    {return (*container_)[offset_ + i];}

    const_reference at(const std::size_t i) const noexcept
    {
        if(nx_ <= i)
        {
            throw std::out_of_range("pnm::image::line_proxy::at: index (" +
                std::to_string(i)   + std::string(") exceeds width(") +
                std::to_string(nx_) + std::string(")"));
        }
        return container_->at(offset_ + i);
    }

    iterator       begin()        noexcept {return container_->begin() + offset_;}
    iterator       end()          noexcept {return container_->begin() + offset_ + nx_;}
    const_iterator begin()  const noexcept {return container_->begin() + offset_;}
    const_iterator end()    const noexcept {return container_->begin() + offset_ + nx_;}
    const_iterator cbegin() const noexcept {return container_->begin() + offset_;}
    const_iterator cend()   const noexcept {return container_->begin() + offset_ + nx_;}

    bool operator==(const const_line_proxy& rhs) const noexcept
    {
        return this->nx_        == rhs.nx_ &&
               this->iy_        == rhs.iy_ &&
               this->container_ == rhs.container_;
    }
    bool operator!=(const const_line_proxy& rhs) const noexcept
    {
        return !(*this == rhs);
    }
    bool operator<(const const_line_proxy& rhs) const noexcept
    {
        return this->nx_ == rhs.nx_ && this->container_ == rhs.container_ &&
               this->iy_ <  rhs.ny_;
    }
    bool operator>(const const_line_proxy& rhs) const noexcept
    {
        return this->nx_ == rhs.nx_ && this->container_ == rhs.container_ &&
               this->iy_ >  rhs.ny_;
    }
    bool operator<=(const const_line_proxy& rhs) const noexcept
    {
        return this->nx_ == rhs.nx_ && this->container_ == rhs.container_ &&
               this->iy_ <= rhs.ny_;
    }
    bool operator>=(const const_line_proxy& rhs) const noexcept
    {
        return this->nx_ == rhs.nx_ && this->container_ == rhs.container_ &&
               this->iy_ >= rhs.ny_;
    }

  private:

    friend struct const_line_proxy_iterator<container_type>;
//     friend struct const_reverse_line_proxy_iterator<container_type>;

    std::size_t           nx_, iy_;
    std::size_t           offset_;
    container_type const* container_;
};

template<typename Container>
struct line_proxy_iterator
{
    using container_type    = Container;
    using value_type        = line_proxy<container_type>;
    using pointer           = value_type const*;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    line_proxy_iterator(container_type& c,
            std::size_t nx, std::size_t ny, std::size_t iy)
        : nx_(nx), ny_(ny), proxy_(c, iy, nx)
    {}
    ~line_proxy_iterator() = default;
    line_proxy_iterator(const line_proxy_iterator&) = default;
    line_proxy_iterator& operator=(const line_proxy_iterator& rhs)
    {
        this->nx_ = rhs.nx_;
        this->ny_ = rhs.ny_;
        this->proxy_.nx_ = rhs.proxy_.nx_;
        this->proxy_.iy_ = rhs.proxy_.iy_;
        this->proxy_.container_ = rhs.proxy_.container_;
        return *this;
    }

    value_type operator* () const noexcept {return value_type(this->proxy_);}
    pointer    operator->() const noexcept {return std::addressof(this->proxy_);}

    line_proxy_iterator& operator++() noexcept
    {
        proxy_.iy_     += 1;
        proxy_.offset_ += nx_;
        return *this;
    }
    line_proxy_iterator& operator--() noexcept
    {
        proxy_.iy_     -= 1;
        proxy_.offset_ -= nx_;
        return *this;
    }
    line_proxy_iterator operator++(int) noexcept
    {
        const auto tmp(*this); ++(*this); return tmp;
    }
    line_proxy_iterator operator--(int) noexcept
    {
        const auto tmp(*this); --(*this); return tmp;
    }

    line_proxy_iterator& operator+=(const difference_type d) noexcept
    {
        proxy_.iy_     += d;
        proxy_.offset_ += nx_ * d;
        return *this;
    }
    line_proxy_iterator& operator-=(const difference_type d) noexcept
    {
        proxy_.iy_     -= d;
        proxy_.offset_ -= nx_ * d;
        return *this;
    }

    bool operator==(const line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ == rhs.proxy_;
    }
    bool operator!=(const line_proxy_iterator& rhs) const noexcept
    {
        return !(*this == rhs);
    }
    bool operator<(const line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ < rhs.proxy_;
    }
    bool operator>(const line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ > rhs.proxy_;
    }
    bool operator<=(const line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ <= rhs.proxy_;
    }
    bool operator>=(const line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ >= rhs.proxy_;
    }

  private:
    std::size_t nx_, ny_;
    value_type  proxy_;
};

template<typename Container>
struct const_line_proxy_iterator
{
    using container_type    = Container;
    using value_type        = const_line_proxy<container_type>;
    using reference         = value_type const&;
    using pointer           = value_type const*;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    const_line_proxy_iterator(const container_type& c,
            std::size_t nx, std::size_t ny, std::size_t iy)
        : nx_(nx), ny_(ny), proxy_(c, iy, nx)
    {}
    ~const_line_proxy_iterator() = default;
    const_line_proxy_iterator(const const_line_proxy_iterator&) = default;
    const_line_proxy_iterator& operator=(const const_line_proxy_iterator& rhs)
    {
        this->nx_ = rhs.nx_;
        this->ny_ = rhs.ny_;
        this->proxy_.nx_ = rhs.proxy_.nx_;
        this->proxy_.iy_ = rhs.proxy_.iy_;
        this->proxy_.container_ = rhs.proxy_.container_;
        return *this;
    }

    const_line_proxy_iterator(const line_proxy_iterator<container_type>& rhs)
        : nx_(rhs.nx_), ny_(rhs.ny_), proxy_(rhs.proxy_)
    {}
    const_line_proxy_iterator&
    operator=(const line_proxy_iterator<container_type>& rhs)
    {
        this->nx_ = rhs.nx_;
        this->ny_ = rhs.ny_;
        this->proxy_.nx_ = rhs.proxy_.nx_;
        this->proxy_.iy_ = rhs.proxy_.iy_;
        this->proxy_.container_ = rhs.proxy_.container_;
        return *this;
    }

    reference operator* () const noexcept {return this->proxy_;}
    pointer   operator->() const noexcept {return std::addressof(this->proxy_);}

    const_line_proxy_iterator& operator++() noexcept
    {
        proxy_.iy_     += 1;
        proxy_.offset_ += nx_;
        return *this;
    }
    const_line_proxy_iterator& operator--() noexcept
    {
        proxy_.iy_     -= 1;
        proxy_.offset_ -= nx_;
        return *this;
    }
    const_line_proxy_iterator operator++(int) noexcept
    {
        const auto tmp(*this); ++(*this); return tmp;
    }
    const_line_proxy_iterator operator--(int) noexcept
    {
        const auto tmp(*this); --(*this); return tmp;
    }

    const_line_proxy_iterator& operator+=(const difference_type d) noexcept
    {
        proxy_.iy_     += d;
        proxy_.offset_ += nx_ * d;
        return *this;
    }
    const_line_proxy_iterator& operator-=(const difference_type d) noexcept
    {
        proxy_.iy_     -= d;
        proxy_.offset_ -= nx_ * d;
        return *this;
    }

    bool operator==(const const_line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ == rhs.proxy_;
    }
    bool operator!=(const const_line_proxy_iterator& rhs) const noexcept
    {
        return !(*this == rhs);
    }
    bool operator<(const const_line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ < rhs.proxy_;
    }
    bool operator>(const const_line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ > rhs.proxy_;
    }
    bool operator<=(const const_line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ <= rhs.proxy_;
    }
    bool operator>=(const const_line_proxy_iterator& rhs) const noexcept
    {
        return this->ny_ == rhs.ny_ && this->proxy_ >= rhs.proxy_;
    }

  private:
    std::size_t nx_, ny_;
    value_type  proxy_;
};

template<typename Iterator>
struct range
{
    using iterator = Iterator;

    range(iterator first, iterator last): begin_(first), end_(last){}
    ~range() = default;
    range(const range&) = default;
    range(range&&)      = default;
    range& operator=(const range&) = default;
    range& operator=(range&&)      = default;

    iterator begin() const noexcept {return this->begin_;}
    iterator end()   const noexcept {return this->end_;}

  private:
    iterator begin_;
    iterator end_;
};

} // detail

// --------------------------------------------------------------------------
//   _                             * pnm::image
//  (_)_ _ _  __ _  __ _  ___    - a container that manages
//  | | ` ` \/ _` |/ _` |/ _ \     - nx, ny
//  | | | | | (_| | (_| |  __/     - vector<pixel>
//  |_|_|_|_|\__,_|\__, |\___|
//                 |___/
// --------------------------------------------------------------------------

template<typename Pixel, typename Alloc = std::allocator<Pixel>>
class image
{
  public:
    using pixel_type             = Pixel;
    using allocator_type         = Alloc;
    using container_type         = std::vector<pixel_type, allocator_type>;
    using size_type              = typename container_type::size_type;
    using difference_type        = typename container_type::difference_type;
    using value_type             = typename container_type::value_type;
    using pointer                = typename container_type::pointer;
    using const_pointer          = typename container_type::const_pointer;
    using reference              = typename container_type::reference;
    using const_reference        = typename container_type::const_reference;
    using iterator               = typename container_type::iterator;
    using const_iterator         = typename container_type::const_iterator;
    using reverse_iterator       = typename container_type::reverse_iterator;
    using const_reverse_iterator = typename container_type::const_reverse_iterator;

    using line_proxy                = detail::line_proxy<container_type>;
    using const_line_proxy          = detail::const_line_proxy<container_type>;
    using line_proxy_iterator       = detail::line_proxy_iterator<container_type>;
    using const_line_proxy_iterator = detail::const_line_proxy_iterator<container_type>;
    using line_range                = detail::range<line_proxy_iterator>;
    using const_line_range          = detail::range<const_line_proxy_iterator>;

    image() = default;
    ~image() = default;
    image(const image&) = default;
    image(image&&)      = default;
    image& operator=(const image&) = default;
    image& operator=(image&&)      = default;

    image(const std::size_t width, const std::size_t height)
        : nx_(width), ny_(height), pixels_(width * height)
    {}

    line_proxy operator[](const std::size_t i) noexcept
    {
        return line_proxy(this->pixels_, i, nx_);
    }
    const_line_proxy operator[](const std::size_t i) const noexcept
    {
        return const_line_proxy(this->pixels_, i, nx_);
    }

    line_proxy at(const std::size_t i)
    {
        if(this->ny_ <= i)
        {
            throw std::out_of_range("pnm::image::at index(" +
                std::to_string(i) + std::string(") exceeds height (") +
                std::to_string(this->ny_) + std::string(") differs"));
        }
        return line_proxy(this->pixels_, i, nx_);
    }
    const_line_proxy at(const std::size_t i) const
    {
        if(this->ny_ <= i)
        {
            throw std::out_of_range("pnm::image::at index(" +
                std::to_string(i) + std::string(") exceeds height (") +
                std::to_string(this->ny_) + std::string(") differs"));
        }
        return const_line_proxy(this->pixels_, i, nx_);
    }

    reference operator()(const std::size_t ix, const std::size_t iy) noexcept
    {
        return pixels_[ix + iy * nx_];
    }
    const_reference
    operator()(const std::size_t ix, const std::size_t iy) const noexcept
    {
        return pixels_[ix + iy * nx_];
    }

    reference at(const std::size_t ix, const std::size_t iy)
    {
        return pixels_.at(ix + iy * nx_);
    }
    const_reference at(const std::size_t ix, const std::size_t iy) const
    {
        return pixels_.at(ix + iy * nx_);
    }

    reference       raw_access(const std::size_t i)       noexcept {return pixels_[i];}
    const_reference raw_access(const std::size_t i) const noexcept {return pixels_[i];}
    reference       raw_at(const std::size_t i)       {return pixels_.at(i);}
    const_reference raw_at(const std::size_t i) const {return pixels_.at(i);}

    std::size_t width()  const noexcept {return nx_;}
    std::size_t height() const noexcept {return ny_;}
    std::size_t x_size() const noexcept {return nx_;}
    std::size_t y_size() const noexcept {return ny_;}

    std::size_t size() const noexcept {return pixels_.size();}

    iterator       begin()        noexcept {return pixels_.begin();}
    iterator       end()          noexcept {return pixels_.end();}
    const_iterator begin()  const noexcept {return pixels_.begin();}
    const_iterator end()    const noexcept {return pixels_.end();}
    const_iterator cbegin() const noexcept {return pixels_.cbegin();}
    const_iterator cend()   const noexcept {return pixels_.cend();}

    reverse_iterator       rbegin()        noexcept {return pixels_.rbegin();}
    reverse_iterator       rend()          noexcept {return pixels_.rend();}
    const_reverse_iterator rbegin()  const noexcept {return pixels_.rbegin();}
    const_reverse_iterator rend()    const noexcept {return pixels_.rend();}
    const_reverse_iterator crbegin() const noexcept {return pixels_.crbegin();}
    const_reverse_iterator crend()   const noexcept {return pixels_.crend();}

    line_proxy_iterator line_begin() noexcept
    {return line_proxy_iterator(pixels_, nx_, ny_, 0);}
    line_proxy_iterator line_end()   noexcept
    {return line_proxy_iterator(pixels_, nx_, ny_, ny_);}

    const_line_proxy_iterator line_begin() const noexcept
    {return const_line_proxy_iterator(pixels_, nx_, ny_, 0);}
    const_line_proxy_iterator line_end()   const noexcept
    {return const_line_proxy_iterator(pixels_, nx_, ny_, ny_);}

    const_line_proxy_iterator line_cbegin() const noexcept
    {return const_line_proxy_iterator(pixels_, nx_, ny_, 0);}
    const_line_proxy_iterator line_cend()   const noexcept
    {return const_line_proxy_iterator(pixels_, nx_, ny_, ny_);}

    line_range lines() noexcept
    {return line_range(this->line_begin(), this->line_end());}
    const_line_range lines() const noexcept
    {return const_line_range(this->line_cbegin(), this->line_cend());}

  private:
    std::size_t   nx_, ny_;
    container_type pixels_;
};

// --------------------------------------------------------------------------
//    __                        _    * io functions and operators
//   / _| ___  _ __ _ _ _  __ _| |_  * enum class format
//  | |_ / _ \| '_/| ` ` \/ _` |  _| * operator<<(ostream, image)
//  |  _| (_) | |  | | | | (_| | |_  * operator>>(ostream, image)
//  |_|  \___/|_|  |_|_|_|\__,_|\__| * image read_*(filename)
//                                   * void write_*(filenmae, image, format_flag = ascii)
// --------------------------------------------------------------------------

enum class format: bool {ascii, binary};

namespace detail
{
// convert pixel value range [0, max] -> [0, 255]
struct gain_base
{
    virtual ~gain_base() noexcept = default;
    virtual std::uint8_t invoke(const std::size_t) const noexcept = 0;
};
struct identity final : gain_base
{
    identity() noexcept = default;
    ~identity() noexcept override = default;
    std::uint8_t invoke(const std::size_t x) const noexcept override
    {return static_cast<std::uint8_t>(x);}
};
struct enlarge final : gain_base
{
    enlarge(const std::uint8_t max) noexcept: ratio(256 / (max+1)){}
    ~enlarge() override = default;

    std::uint8_t invoke(const std::size_t x) const noexcept override
    {
        return static_cast<std::uint8_t>(x * ratio);
    }
    std::size_t ratio;
};
struct reduce final : gain_base
{
    reduce(const std::size_t max) noexcept: ratio(256.0 / (max+1.0)){}
    ~reduce() override = default;

    std::uint8_t invoke(const std::size_t x) const noexcept override
    {
        return static_cast<std::uint8_t>(x * ratio);
    }
    double ratio;
};

inline std::unique_ptr<gain_base> get_gain(const std::size_t max)
{
    if     (max == 255){return std::unique_ptr<identity>(new identity());}
    else if(max >  255){return std::unique_ptr<reduce>  (new reduce(max));}
    else               {return std::unique_ptr<enlarge> (new enlarge(max));}
}

namespace literals
{
inline std::string operator"" _str(const char* s, std::size_t len)
{
    return std::string(s, len);
}
} // literals
} // detail

// --------------------------------------------------------------------------
//                     _
//  _ __ ___  __ _  __| |
// | '_// _ \/ _` |/ _` |
// | | (  __/ (_| | (_| |
// |_|  \___|\__,_|\__,_|
// --------------------------------------------------------------------------

template<typename Alloc = std::allocator<bit_pixel>>
image<bit_pixel, Alloc> read_pbm_ascii(const std::string& fname)
{
    using detail::literals::operator"" _str;

    std::ifstream ifs(fname);
    if(!ifs.good())
    {
        throw std::runtime_error(
                "pnm::read_pbm_ascii: file open error: "_str + fname);
    }

    {
        char desc[2] = {'\0', '\0'};
        ifs.read(desc, 2);
        if(!(desc[0] == 'P' && desc[1] == '1'))
        {
            throw std::runtime_error("pnm::read_pbm_ascii: " + fname +
                " is not a pbm file: magic number is "_str +
                std::string{desc[0], desc[1]});
        }
    }

    bool        x_read(false), y_read(false);
    std::size_t x(0),          y(0);
    while(!ifs.eof())
    {
        std::string line;
        std::getline(ifs, line);
        line.erase(std::find(line.begin(), line.end(), '#'), line.end());
        if(line.empty()){continue;}

        std::size_t tmp;
        std::istringstream iss(line);
        while(!iss.eof())
        {
            iss >> tmp;
            if(iss.fail())
            {
                std::string dummy;
                iss >> dummy;
                if(!std::all_of(dummy.begin(), dummy.end(), [](const char c){
                        return std::isspace(static_cast<int>(c));
                    }))
                {
                    throw std::runtime_error("pnm::read_pbm_ascii: file " +
                        fname + " contains invalid token: "_str + dummy);
                }
            }
            else
            {
                if(!x_read){x = tmp; x_read = true; continue;}
                if(!y_read){y = tmp; y_read = true; continue;}
            }
        }
        if(x_read && y_read)
        {
            break;
        }
    }

    image<bit_pixel, Alloc> img(x, y);

    std::size_t idx=0;
    while(!ifs.eof())
    {
        std::string line;
        std::getline(ifs, line);
        line.erase(std::find(line.begin(), line.end(), '#'), line.end());
        if(line.empty()){continue;}

        int pix;
        std::istringstream iss(line);
        while(!iss.eof())
        {
            iss >> pix;
            if(iss.fail())
            {
                std::string dummy;
                iss >> dummy;
                if(!std::all_of(dummy.begin(), dummy.end(), [](const char c){
                        return std::isspace(static_cast<int>(c));
                    }))
                {
                    throw std::runtime_error("pnm::read_pbm_ascii: "
                        "file contains invalid token: " + dummy);
                }
            }
            else
            {
                if(idx >= x * y)
                {
                    throw std::runtime_error("pnm::read_pbm_ascii: file "  +
                        fname + " contains too many pixels: "_str +
                        std::to_string(idx) + " pixels for "_str  +
                        std::to_string(x)   + "x"_str             +
                        std::to_string(y)   + " image"_str);
                }
                img.raw_access(idx++) = bit_pixel(pix != 0);
            }
        }
    }
    return img;
}
template<typename Alloc = std::allocator<bit_pixel>>
image<bit_pixel, Alloc> read_pbm_binary(const std::string& fname)
{
    using detail::literals::operator"" _str;
    std::ifstream ifs(fname);
    if(!ifs.good())
    {
        throw std::runtime_error(
                "pnm::read_pbm_binary: file open error: " + fname);
    }

    {
        std::string desc;
        std::getline(ifs, desc);
        if(desc != "P4")
        {
            throw std::runtime_error("pnm::read_pbm_binary: " + fname +
                " is not a binary pbm file: magic number is "_str + desc);
        }
    }

    std::size_t x(0), y(0);
    {
        std::string line;
        std::getline(ifs, line);
        std::istringstream iss(line);
        iss >> x >> y;
        if(iss.fail())
        {
            throw std::runtime_error("pnm::read_pbm_binary: "
                "couldn't read file size: " + line + " from "_str + fname);
        }
    }
    image<bit_pixel, Alloc> img(x, y);

    const std::size_t quot = x >> 3u;
    const std::size_t rem  = x &  7u;

    for(std::size_t j=0; j<y; ++j)
    {
        for(std::size_t i=0; i<quot; ++i)
        {
            std::uint8_t v(0);
            ifs.read(reinterpret_cast<char*>(&v), 1);
            img(i*8 + 0, j) = bit_pixel((v & 0x80) == 0x80);
            img(i*8 + 1, j) = bit_pixel((v & 0x40) == 0x40);
            img(i*8 + 2, j) = bit_pixel((v & 0x20) == 0x20);
            img(i*8 + 3, j) = bit_pixel((v & 0x10) == 0x10);
            img(i*8 + 4, j) = bit_pixel((v & 0x08) == 0x08);
            img(i*8 + 5, j) = bit_pixel((v & 0x04) == 0x04);
            img(i*8 + 6, j) = bit_pixel((v & 0x02) == 0x02);
            img(i*8 + 7, j) = bit_pixel((v & 0x01) == 0x01);
        }
        std::uint8_t v(0);
        ifs.read(reinterpret_cast<char*>(&v), 1);
        switch(rem)
        {
            case 7: img(quot*8+7,j)=bit_pixel((v&0x02)==0x02); //[[fallthrough]]
            case 6: img(quot*8+6,j)=bit_pixel((v&0x04)==0x04); //[[fallthrough]]
            case 5: img(quot*8+5,j)=bit_pixel((v&0x08)==0x08); //[[fallthrough]]
            case 4: img(quot*8+4,j)=bit_pixel((v&0x10)==0x10); //[[fallthrough]]
            case 3: img(quot*8+3,j)=bit_pixel((v&0x20)==0x20); //[[fallthrough]]
            case 2: img(quot*8+2,j)=bit_pixel((v&0x40)==0x40); //[[fallthrough]]
            case 1: img(quot*8+1,j)=bit_pixel((v&0x80)==0x80); //[[fallthrough]]
            default: break;
        }
    }
    return img;
}

template<typename Alloc = std::allocator<bit_pixel>>
image<bit_pixel, Alloc> read_pbm(const std::string& fname)
{
    using detail::literals::operator"" _str;
    char descripter[2];
    {
        std::ifstream ifs(fname, std::ios::binary);
        ifs.read(descripter, 2);
    }

    if(descripter[0] == 'P' && descripter[1] == '1')
    {
        return read_pbm_ascii(fname);
    }
    else if(descripter[0] == 'P' && descripter[1] == '4')
    {
        return read_pbm_binary(fname);
    }
    throw std::runtime_error("pnm::read_pbm: not a pbm file: magic number is " +
                             std::string{descripter[0], descripter[1]});
}

template<typename Alloc = std::allocator<gray_pixel>>
image<gray_pixel, Alloc> read_pgm_ascii(const std::string& fname)
{
    using detail::literals::operator"" _str;
    std::ifstream ifs(fname);
    if(!ifs.good())
    {
        throw std::runtime_error(
                "pnm::read_pgm_ascii: file open error: " + fname);
    }

    {
        char desc[2] = {'\0', '\0'};
        ifs.read(desc, 2);
        if(!(desc[0] == 'P' && desc[1] == '2'))
        {
            throw std::runtime_error("pnm::read_pgm_ascii: " + fname +
                "not a pgm file: magic number is "_str +
                std::string{desc[0], desc[1]});
        }
    }

    bool        x_read(false), y_read(false), max_read(false);
    std::size_t x(0),          y(0),          max(0);
    while(!ifs.eof())
    {
        std::string line;
        std::getline(ifs, line);
        line.erase(std::find(line.begin(), line.end(), '#'), line.end());
        if(line.empty()){continue;}

        std::size_t tmp;
        std::istringstream iss(line);
        while(!iss.eof())
        {
            iss >> tmp;
            if(iss.fail())
            {
                std::string dummy;
                iss >> dummy;
                if(!std::all_of(dummy.begin(), dummy.end(), [](const char c){
                        return std::isspace(static_cast<int>(c));
                    }))
                {
                    throw std::runtime_error("pnm::read_pgm_ascii: file " +
                        fname + " contains invalid token: "_str  + dummy);
                }
            }
            else
            {
                if(  !x_read){  x = tmp;   x_read = true; continue;}
                if(  !y_read){  y = tmp;   y_read = true; continue;}
                if(!max_read){max = tmp; max_read = true; continue;}
            }
        }
        if(x_read && y_read && max_read)
        {
            break;
        }
    }

    image<gray_pixel, Alloc> img(x, y);
    const auto gain = detail::get_gain(max);

    std::size_t idx=0;
    while(!ifs.eof())
    {
        std::string line;
        std::getline(ifs, line);
        line.erase(std::find(line.begin(), line.end(), '#'), line.end());
        if(line.empty()){continue;}

        std::size_t pix;
        std::istringstream iss(line);
        while(!iss.eof())
        {
            iss >> pix;
            if(iss.fail())
            {
                std::string dummy;
                iss >> dummy;
                if(!std::all_of(dummy.begin(), dummy.end(), [](const char c){
                        return std::isspace(static_cast<int>(c));
                    }))
                {
                    throw std::runtime_error("pnm::read_pgm_ascii: file " +
                        fname + " contains invalid token: "_str  + dummy);
                }
            }
            else
            {
                if(idx >= x * y)
                {
                    throw std::runtime_error("pnm::read_pgm_ascii: file "  +
                        fname + "contains too many pixels: "_str  +
                        std::to_string(idx) + " pixels for "_str  +
                        std::to_string(x)   + "x"_str             +
                        std::to_string(y)   + " image"_str);
                }
                img.raw_access(idx++) = gray_pixel(gain->invoke(pix));
            }
        }
    }
    return img;
}

template<typename Alloc = std::allocator<gray_pixel>>
image<gray_pixel, Alloc> read_pgm_binary(const std::string& fname)
{
    using detail::literals::operator"" _str;
    std::ifstream ifs(fname);
    if(!ifs.good())
    {
        throw std::runtime_error(
                "pnm::read_pgm_binary: file open error: " + fname);
    }

    {
        std::string desc;
        std::getline(ifs, desc);
        if(desc != "P5")
        {
            throw std::runtime_error("pnm::read_pgm_binary: " + fname +
                " is not a binary pgm file: magic number is "_str + desc);
        }
    }

    std::size_t x(0), y(0), max(0);
    {
        std::string line;
        std::getline(ifs, line);
        std::istringstream iss(line);
        iss >> x >> y;
        if(iss.fail())
        {
            throw std::runtime_error("pnm::read_pgm_binary: "
                "couldn't read file size: " + line + " from "_str + fname);
        }
    }
    {
        std::string line;
        std::getline(ifs, line);
        std::istringstream iss(line);
        iss >> max;
        if(iss.fail())
        {
            throw std::runtime_error("pnm::read_pgm_binary: "
                "couldn't read max value: " + line + " from "_str + fname);
        }
    }

    image<gray_pixel, Alloc> img(x, y);
    const auto gain = detail::get_gain(max);

    for(std::size_t i=0; i<img.size(); ++i)
    {
        std::uint8_t v(0);
        ifs.read(reinterpret_cast<char*>(&v), 1);
        img.raw_access(i) = gray_pixel(gain->invoke(v));
    }
    return img;
}

template<typename Alloc = std::allocator<gray_pixel>>
image<gray_pixel, Alloc> read_pgm(const std::string& fname)
{
    using detail::literals::operator"" _str;
    char descripter[2];
    {
        std::ifstream ifs(fname, std::ios::binary);
        ifs.read(descripter, 2);
    }

    if(descripter[0] == 'P' && descripter[1] == '2')
    {
        return read_pgm_ascii(fname);
    }
    else if(descripter[0] == 'P' && descripter[1] == '5')
    {
        return read_pgm_binary(fname);
    }
    throw std::runtime_error("pnm::read_pgm: " + fname +
        " is not a pgm file: magic number is "_str +
        std::string{descripter[0], descripter[1]});
}

template<typename Alloc = std::allocator<rgb_pixel>>
image<rgb_pixel, Alloc> read_ppm_ascii(const std::string& fname)
{
    using detail::literals::operator"" _str;
    std::ifstream ifs(fname);
    if(!ifs.good())
    {
        throw std::runtime_error(
                "pnm::read_ppm_ascii: file open error: " + fname);
    }

    {
        char desc[2] = {'\0', '\0'};
        ifs.read(desc, 2);
        if(!(desc[0] == 'P' && desc[1] == '3'))
        {
            throw std::runtime_error("pnm::read_ppm_ascii: " + fname +
                " is not a ppm file: magic number is "_str +
                std::string{desc[0], desc[1]});
        }
    }

    bool        x_read(false), y_read(false), max_read(false);
    std::size_t x(0),          y(0),          max(0);
    while(!ifs.eof())
    {
        std::string line;
        std::getline(ifs, line);
        line.erase(std::find(line.begin(), line.end(), '#'), line.end());
        if(line.empty()){continue;}

        std::size_t tmp;
        std::istringstream iss(line);
        while(!iss.eof())
        {
            iss >> tmp;
            if(iss.fail())
            {
                std::string dummy;
                iss >> dummy;
                if(!std::all_of(dummy.begin(), dummy.end(), [](const char c){
                        return std::isspace(static_cast<int>(c));
                    }))
                {
                    throw std::runtime_error("pnm::read_ppm_ascii: file " +
                        fname + " contains invalid token: "_str + dummy);
                }
            }
            else
            {
                if(  !x_read){  x = tmp;   x_read = true; continue;}
                if(  !y_read){  y = tmp;   y_read = true; continue;}
                if(!max_read){max = tmp; max_read = true; continue;}
            }
        }
        if(x_read && y_read && max_read)
        {
            break;
        }
    }

    image<rgb_pixel, Alloc> img(x, y);
    const auto gain = detail::get_gain(max);

    bool R_read(false), G_read(false), B_read(false);
    std::size_t idx=0;
    rgb_pixel pixel;

    while(!ifs.eof())
    {
        std::string line;
        std::getline(ifs, line);
        line.erase(std::find(line.begin(), line.end(), '#'), line.end());
        if(line.empty()){continue;}

        std::size_t pix;
        std::istringstream iss(line);
        while(!iss.eof())
        {
            iss >> pix;
            if(iss.fail())
            {
                std::string dummy;
                iss >> dummy;
                if(!std::all_of(dummy.begin(), dummy.end(), [](const char c){
                        return std::isspace(static_cast<int>(c));
                    }))
                {
                    throw std::runtime_error("pnm::read_ppm_ascii: file " +
                        fname + " contains invalid token: "_str + dummy);
                }
            }
            else
            {
                if(!R_read){pixel.red   = gain->invoke(pix); R_read = true; continue;}
                if(!G_read){pixel.green = gain->invoke(pix); G_read = true; continue;}
                if(!B_read){pixel.blue  = gain->invoke(pix); B_read = true;}
                R_read = false;
                G_read = false;
                B_read = false;
                if(idx >= x * y)
                {
                    throw std::runtime_error("pnm::read_ppm_ascii: file " +
                        fname + "contains too many pixels: "_str +
                        std::to_string(idx) + " pixels for "_str +
                        std::to_string(x)   + "x"_str            +
                        std::to_string(y)   + " image"_str);
                }
                img.raw_access(idx++) = pixel;
            }
        }
    }
    return img;
}

template<typename Alloc = std::allocator<rgb_pixel>>
image<rgb_pixel, Alloc> read_ppm_binary(const std::string& fname)
{
    using detail::literals::operator"" _str;
    std::ifstream ifs(fname);
    if(!ifs.good())
    {
        throw std::runtime_error(
                "pnm::read_ppm_binary: file open error: " + fname);
    }

    {
        std::string desc;
        std::getline(ifs, desc);
        if(desc != "P6")
        {
            throw std::runtime_error("pnm::read_ppm_binary: " + fname +
                " is not a binary ppm file: magic number is "_str + desc);
        }
    }

    std::size_t x(0), y(0), max(0);
    {
        std::string line;
        std::getline(ifs, line);
        std::istringstream iss(line);
        iss >> x >> y;
        if(iss.fail())
        {
            throw std::runtime_error("pnm::read_ppm_binary: "
                "couldn't read file size: " + line + " from "_str + fname);
        }
    }
    {
        std::string line;
        std::getline(ifs, line);
        std::istringstream iss(line);
        iss >> max;
        if(iss.fail())
        {
            throw std::runtime_error("pnm::read_ppm_binary: "
                "couldn't read max value: " + line + " form "_str + fname);
        }
    }

    image<rgb_pixel, Alloc> img(x, y);
    const auto gain = detail::get_gain(max);

    for(std::size_t i=0; i<img.size(); ++i)
    {
        std::uint8_t R(0), G(0), B(0);
        ifs.read(reinterpret_cast<char*>(&R), 1);
        ifs.read(reinterpret_cast<char*>(&G), 1);
        ifs.read(reinterpret_cast<char*>(&B), 1);
        img.raw_access(i) =
            rgb_pixel(gain->invoke(R), gain->invoke(G), gain->invoke(B));
    }
    return img;
}

template<typename Alloc = std::allocator<rgb_pixel>>
image<rgb_pixel, Alloc> read_ppm(const std::string& fname)
{
    using detail::literals::operator"" _str;
    char descripter[2];
    {
        std::ifstream ifs(fname, std::ios::binary);
        ifs.read(descripter, 2);
    }

    if(descripter[0] == 'P' && descripter[1] == '3')
    {
        return read_ppm_ascii(fname);
    }
    else if(descripter[0] == 'P' && descripter[1] == '6')
    {
        return read_ppm_binary(fname);
    }
    throw std::runtime_error("pnm::read_ppm: " + fname +
            " is not a ppm file: magic number is "_str +
            std::string{descripter[0], descripter[1]});
}

// --------------------------------------------------------------------------
//                  _ _
//  __      __ _ __(_) |_  ___
//  \ \ /\ / /| '_/| | __|/ _ \
//   \ v  v / | |  | | |_(  __/
//    \_/\_/  |_|  |_|\__|\___|
// --------------------------------------------------------------------------

template<typename Alloc>
void write_pbm_ascii(const std::string& fname,
                     const image<bit_pixel, Alloc>& img)
{
    std::ofstream ofs(fname);
    if(!ofs.good())
    {
        throw std::runtime_error(
                "pnm::write_pbm_ascii: file open error: " + fname);
    }

    ofs << "P1\n" << img.x_size() << ' ' << img.y_size() << "\n";

    for(std::size_t j=0; j<img.y_size(); ++j)
    {
        for(std::size_t i=0; i<img.x_size(); ++i)
        {
            ofs << (img(i, j).value ? '1' : '0') << ' ';
        }
        ofs << '\n';
    }
    return ;
}

template<typename Alloc>
void write_pbm_binary(const std::string& fname,
                      const image<bit_pixel, Alloc>& img)
{
    std::ofstream ofs(fname);
    if(!ofs.good())
    {
        throw std::runtime_error(
                "pnm::write_pbm_binary: file open error: " + fname);
    }

    ofs << "P4\n" << img.x_size() << ' ' << img.y_size() << "\n";

    const auto get_or = [](const std::size_t i, const std::size_t j,
                           const image<bit_pixel, Alloc>& im) -> bool {
        if(i < im.x_size() && j < im.y_size()) {return im(i, j).value;}
        return false;
    };

    for(std::size_t j=0; j<img.y_size(); ++j)
    {
        std::uint8_t buf(0u);
        for(std::size_t i=0; i<img.x_size(); i+=8)
        {
            if(get_or(i+0, j, img)){buf |= 0x80;}
            if(get_or(i+1, j, img)){buf |= 0x40;}
            if(get_or(i+2, j, img)){buf |= 0x20;}
            if(get_or(i+3, j, img)){buf |= 0x10;}
            if(get_or(i+4, j, img)){buf |= 0x08;}
            if(get_or(i+5, j, img)){buf |= 0x04;}
            if(get_or(i+6, j, img)){buf |= 0x02;}
            if(get_or(i+7, j, img)){buf |= 0x01;}
            ofs.write(reinterpret_cast<const char*>(&buf), 1);
        }
    }
    return ;
}

template<typename Alloc>
void write_pbm(const std::string& fname, const image<bit_pixel, Alloc>& img,
               const format fmt = format::ascii)
{
    if(fmt == format::ascii)
    {
        return write_pbm_ascii(fname, img);
    }
    else if(fmt == format::binary)
    {
        return write_pbm_binary(fname, img);
    }
    throw std::runtime_error("pnm::write_pbm: "
            "invalid format flag (neither ascii nor binary)");
}

template<typename Alloc>
void write_pgm_ascii(const std::string& fname,
                     const image<gray_pixel, Alloc>& img)
{
    std::ofstream ofs(fname);
    if(!ofs.good())
    {
        throw std::runtime_error(
                "pnm::write_pgm_ascii: file open error: " + fname);
    }

    ofs << "P2\n" << img.x_size() << ' ' << img.y_size() << "\n256\n";

    for(std::size_t j=0; j<img.y_size(); ++j)
    {
        for(std::size_t i=0; i<img.x_size(); ++i)
        {
            ofs << static_cast<int>(img(i, j).value) << ' ';
        }
        ofs << '\n';
    }
    return ;
}

template<typename Alloc>
void write_pgm_binary(const std::string& fname,
                      const image<gray_pixel, Alloc>& img)
{
    std::ofstream ofs(fname);
    if(!ofs.good())
    {
        throw std::runtime_error(
                "pnm::write_pgm_ascii: file open error: " + fname);
    }

    ofs << "P5\n" << img.x_size() << ' ' << img.y_size() << "\n256\n";

    for(std::size_t j=0; j<img.y_size(); ++j)
    {
        for(std::size_t i=0; i<img.x_size(); ++i)
        {
            ofs.write(reinterpret_cast<const char*>(
                        std::addressof(img(i, j).value)), 1);
        }
        ofs << '\n';
    }
    return ;
}

template<typename Alloc>
void write_pgm(const std::string& fname, const image<gray_pixel, Alloc>& img,
               const format fmt = format::ascii)
{
    if(fmt == format::ascii)
    {
        return write_pgm_ascii(fname, img);
    }
    else if(fmt == format::binary)
    {
        return write_pgm_binary(fname, img);
    }
    throw std::runtime_error("pnm::write_pgm: "
            "invalid format flag (neither ascii nor binary)");
}


template<typename Alloc>
void write_ppm_ascii(const std::string& fname,
                     const image<rgb_pixel, Alloc>& img)
{
    std::ofstream ofs(fname);
    if(!ofs.good())
    {
        throw std::runtime_error(
                "pnm::write_ppm_ascii: file open error: " + fname);
    }

    ofs << "P3\n" << img.x_size() << ' ' << img.y_size() << "\n255\n";

    for(std::size_t j=0; j<img.y_size(); ++j)
    {
        for(std::size_t i=0; i<img.x_size(); ++i)
        {
            const auto& pixel = img(i, j);
            ofs << static_cast<int>(pixel.red)   << ' '
                << static_cast<int>(pixel.green) << ' '
                << static_cast<int>(pixel.blue)  << ' ';
        }
        ofs << '\n';
    }
    return ;
}

template<typename Alloc>
void write_ppm_binary(const std::string& fname,
                      const image<rgb_pixel, Alloc>& img)
{
    std::ofstream ofs(fname);
    if(!ofs.good())
    {
        throw std::runtime_error(
                "pnm::write_ppm_binary: file open error: " + fname);
    }

    ofs << "P6\n" << img.x_size() << ' ' << img.y_size() << "\n255\n";

    for(std::size_t j=0; j<img.y_size(); ++j)
    {
        for(std::size_t i=0; i<img.x_size(); ++i)
        {
            const auto& pixel = img(i, j);
            ofs.write(reinterpret_cast<const char*>(std::addressof(pixel.red)),   1);
            ofs.write(reinterpret_cast<const char*>(std::addressof(pixel.green)), 1);
            ofs.write(reinterpret_cast<const char*>(std::addressof(pixel.blue)),  1);
        }
    }
    return ;
}

template<typename Alloc>
void write_ppm(const std::string& fname, const image<rgb_pixel, Alloc>& img,
               const format fmt = format::ascii)
{
    if(fmt == format::ascii)
    {
        return write_ppm_ascii(fname, img);
    }
    else if(fmt == format::binary)
    {
        return write_ppm_binary(fname, img);
    }
    throw std::runtime_error("pnm::write_ppm: "
            "invalid format flag (neither ascii nor binary)");
}

} // pnm
#endif // PNM_PLUS_PLUS_H