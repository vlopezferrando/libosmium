#include "catch.hpp"

#include <osmium/index/map/dense_file_array.hpp>
#include <osmium/index/map/dense_mem_array.hpp>
#include <osmium/index/map/dense_mmap_array.hpp>
#include <osmium/index/map/dummy.hpp>
#include <osmium/index/map/flex_mem.hpp>
#include <osmium/index/map/sparse_file_array.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/index/map/sparse_mem_compact_array.hpp>
#include <osmium/index/map/sparse_mem_map.hpp>
#include <osmium/index/map/sparse_mem_table.hpp>
#include <osmium/index/map/sparse_mmap_array.hpp>
#include <osmium/index/node_locations_map.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/types.hpp>

#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

static_assert(osmium::index::empty_value<osmium::Location>() == osmium::Location{}, "Empty value for location is wrong");

template <typename TIndex>
void test_func_all(TIndex& index) {
    const osmium::unsigned_object_id_type id1 = 12;
    const osmium::unsigned_object_id_type id2 = 3;
    const osmium::Location loc1{1.2, 4.5};
    const osmium::Location loc2{3.5, -7.2};

    REQUIRE_THROWS_AS(index.get(id1), osmium::not_found);

    index.set(id1, loc1);
    index.set(id2, loc2);

    index.sort();

    REQUIRE_THROWS_AS(index.get(0), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(1), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(5), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(100), osmium::not_found);
    REQUIRE_THROWS_WITH(index.get(0), "id 0 not found");
    REQUIRE_THROWS_WITH(index.get(1), "id 1 not found");

    REQUIRE(index.get_noexcept(0) == osmium::Location{});
    REQUIRE(index.get_noexcept(1) == osmium::Location{});
    REQUIRE(index.get_noexcept(5) == osmium::Location{});
    REQUIRE(index.get_noexcept(100) == osmium::Location{});
}

osmium::Location location_matching_id(osmium::unsigned_object_id_type id) {
    return osmium::Location(int32_t(id), int32_t(id));
}

template <typename TIndex>
void check_ids(TIndex& index, std::vector<bool>& set_ids) {
    for (osmium::unsigned_object_id_type id = 0; id < set_ids.size(); ++id) {
        if (set_ids[id]) {
            REQUIRE(location_matching_id(id) == index.get(id));
            REQUIRE(location_matching_id(id) == index.get_noexcept(id));
        }
        else {
            REQUIRE_THROWS_AS(index.get(id), osmium::not_found);
            REQUIRE(index.get_noexcept(id) == osmium::Location{});
        }
    }
}

template <typename TIndex>
void test_func_real(TIndex& index) {
    const std::vector<osmium::unsigned_object_id_type> ordered_ids{0, 1, 2, 3, 10, 11, 12, 20, 30, 31};
    const std::vector<osmium::unsigned_object_id_type> unordered_ids{40, 13, 6, 7, 8, 39, 25};

    // Vector of booleans that keeps track of which IDs have been set in the index
    osmium::unsigned_object_id_type max_n_ids = 50;
    std::vector<bool> set_ids(max_n_ids, false);

    check_ids<TIndex>(index, set_ids);

    // Set ordered IDs and remove from missing_ids
    for (const auto id : ordered_ids) {
        index.set(id, location_matching_id(id));
        set_ids[id] = true;
    }

    check_ids<TIndex>(index, set_ids);

    // Set unordered IDs and remove from missing_ids
    for (const auto id : unordered_ids) {
        index.set(id, location_matching_id(id));
        set_ids[id] = true;
    }

    // Sort
    index.sort();

    check_ids<TIndex>(index, set_ids);

    // Clear index
    index.clear();
    set_ids = std::vector<bool>(max_n_ids, false);

    REQUIRE(0 == index.size());

    check_ids<TIndex>(index, set_ids);
}

template <typename TIndex>
void test_func_limits(TIndex& index) {
    const auto max_id = std::numeric_limits<osmium::unsigned_object_id_type>::max();

    const osmium::unsigned_object_id_type id1 = 0;
    const osmium::unsigned_object_id_type id2 = max_id-5;
    const osmium::unsigned_object_id_type id3 = max_id/2;
    const osmium::Location loc1{1.2, 4.5};
    const osmium::Location loc2{3.5, -7.2};
    const osmium::Location loc3{42.0, -12.3};

    index.set(id1, loc1);
    index.set(id2, loc2);
    index.set(id3, loc3);

    index.sort();

    REQUIRE(loc1 == index.get(id1));
    REQUIRE(loc2 == index.get(id2));
    REQUIRE(loc3 == index.get(id3));

    REQUIRE(loc1 == index.get_noexcept(id1));
    REQUIRE(loc2 == index.get_noexcept(id2));
    REQUIRE(loc3 == index.get_noexcept(id3));

    REQUIRE_THROWS_AS(index.get(1), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(5), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(100), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(max_id-1), osmium::not_found);

    REQUIRE(index.get_noexcept(1) == osmium::Location{});
    REQUIRE(index.get_noexcept(5) == osmium::Location{});
    REQUIRE(index.get_noexcept(100) == osmium::Location{});
    REQUIRE(index.get_noexcept(max_id-1) == osmium::Location{});

    index.clear();

    REQUIRE_THROWS_AS(index.get(id1), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(id2), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(id3), osmium::not_found);

    REQUIRE_THROWS_AS(index.get(0), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(1), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(5), osmium::not_found);
    REQUIRE_THROWS_AS(index.get(100), osmium::not_found);

    REQUIRE(index.get_noexcept(id1) == osmium::Location{});
    REQUIRE(index.get_noexcept(id2) == osmium::Location{});
    REQUIRE(index.get_noexcept(id3) == osmium::Location{});

    REQUIRE(index.get_noexcept(1) == osmium::Location{});
    REQUIRE(index.get_noexcept(5) == osmium::Location{});
    REQUIRE(index.get_noexcept(100) == osmium::Location{});
    REQUIRE(index.get_noexcept(max_id-1) == osmium::Location{});
}

TEST_CASE("Map Id to location: Dummy") {
    using index_type = osmium::index::map::Dummy<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;

    REQUIRE(0 == index1.size());
    REQUIRE(0 == index1.used_memory());

    test_func_all<index_type>(index1);

    REQUIRE(0 == index1.size());
    REQUIRE(0 == index1.used_memory());
}

TEST_CASE("Map Id to location: DenseMemArray") {
    using index_type = osmium::index::map::DenseMemArray<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;
    index1.reserve(1000);
    test_func_all<index_type>(index1);

    index_type index2;
    index2.reserve(1000);
    test_func_real<index_type>(index2);
}

#ifdef __linux__
TEST_CASE("Map Id to location: DenseMmapArray") {
    using index_type = osmium::index::map::DenseMmapArray<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;
    test_func_all<index_type>(index1);

    index_type index2;
    test_func_real<index_type>(index2);
}
#else
# pragma message("not running 'DenseMmapArray' test case on this machine")
#endif

TEST_CASE("Map Id to location: DenseFileArray") {
    using index_type = osmium::index::map::DenseFileArray<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;
    test_func_all<index_type>(index1);

    index_type index2;
    test_func_real<index_type>(index2);
}

#ifdef OSMIUM_WITH_SPARSEHASH

TEST_CASE("Map Id to location: SparseMemTable") {
    using index_type = osmium::index::map::SparseMemTable<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;
    test_func_all<index_type>(index1);

    index_type index2;
    test_func_real<index_type>(index2);

    index_type index3;
    test_func_limits<index_type>(index3);
}

#endif

TEST_CASE("Map Id to location: SparseMemMap") {
    using index_type = osmium::index::map::SparseMemMap<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;
    test_func_all<index_type>(index1);

    index_type index2;
    test_func_real<index_type>(index2);

    index_type index3;
    test_func_limits<index_type>(index3);
}

TEST_CASE("Map Id to location: SparseMemArray") {
    using index_type = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;

    REQUIRE(0 == index1.size());
    REQUIRE(0 == index1.used_memory());

    test_func_all<index_type>(index1);

    REQUIRE(2 == index1.size());

    index_type index2;
    test_func_real<index_type>(index2);

    index_type index3;
    test_func_limits<index_type>(index3);
}

TEST_CASE("Map Id to location: SparseMemCompactArray") {
  using index_type = osmium::index::map::SparseMemCompactArray<osmium::unsigned_object_id_type, osmium::Location>;

  index_type index1;

  REQUIRE(0 == index1.size());
  REQUIRE(0 == index1.used_memory());

  test_func_all<index_type>(index1);

  REQUIRE(2 == index1.size());

  index_type index2;
  test_func_real<index_type>(index2);

  index_type index3;
  test_func_limits<index_type>(index3);
}

#ifdef __linux__
TEST_CASE("Map Id to location: SparseMmapArray") {
    using index_type = osmium::index::map::SparseMmapArray<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;
    test_func_all<index_type>(index1);

    index_type index2;
    test_func_real<index_type>(index2);

    index_type index3;
    test_func_limits<index_type>(index3);
}
#else
# pragma message("not running 'SparseMmapArray' test case on this machine")
#endif

TEST_CASE("Map Id to location: FlexMem sparse") {
    using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1;
    test_func_all<index_type>(index1);

    index_type index2;
    test_func_real<index_type>(index2);

    index_type index3;
    test_func_limits<index_type>(index3);
}

TEST_CASE("Map Id to location: FlexMem dense") {
    using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;

    index_type index1{true};
    test_func_all<index_type>(index1);

    index_type index2{true};
    test_func_real<index_type>(index2);
}

TEST_CASE("Map Id to location: FlexMem switch") {
    using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;

    const osmium::Location loc1{1.1, 1.2};
    const osmium::Location loc2{2.2, -9.4};

    index_type index;

    REQUIRE(index.size() == 0);

    index.set(17, loc1);
    index.set(99, loc2);

    REQUIRE_FALSE(index.is_dense());
    REQUIRE(index.size() == 2);
    REQUIRE(index.get_noexcept(0) == osmium::Location{});
    REQUIRE(index.get_noexcept(1) == osmium::Location{});
    REQUIRE(index.get_noexcept(17) == loc1);
    REQUIRE(index.get_noexcept(99) == loc2);
    REQUIRE(index.get_noexcept(2000000000) == osmium::Location{});

    index.switch_to_dense();

    REQUIRE(index.is_dense());
    REQUIRE(index.size() >= 2);
    REQUIRE(index.get_noexcept(0) == osmium::Location{});
    REQUIRE(index.get_noexcept(1) == osmium::Location{});
    REQUIRE(index.get_noexcept(17) == loc1);
    REQUIRE(index.get_noexcept(99) == loc2);
    REQUIRE(index.get_noexcept(2000000000) == osmium::Location{});
}

TEST_CASE("Map Id to location: Dynamic map choice") {
    using map_type = osmium::index::map::Map<osmium::unsigned_object_id_type, osmium::Location>;
    const auto& map_factory = osmium::index::MapFactory<osmium::unsigned_object_id_type, osmium::Location>::instance();

    const std::vector<std::string> map_type_names = map_factory.map_types();
    REQUIRE(map_type_names.size() >= 6);

    REQUIRE_THROWS_AS(map_factory.create_map(""), osmium::map_factory_error);
    REQUIRE_THROWS_AS(map_factory.create_map("does not exist"), osmium::map_factory_error);
    REQUIRE_THROWS_WITH(map_factory.create_map(""), "Need non-empty map type name");
    REQUIRE_THROWS_WITH(map_factory.create_map("does not exist"), "Support for map type 'does not exist' not compiled into this binary");

    for (const auto& map_type_name : map_type_names) {
       std::unique_ptr<map_type> index1 =
          map_factory.create_map(map_type_name);
      index1->reserve(1000);
      test_func_all<map_type>(*index1);

      std::unique_ptr<map_type> index2 = map_factory.create_map(map_type_name);
      index2->reserve(1000);
      test_func_real<map_type>(*index2);
    }
}

