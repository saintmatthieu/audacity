#include "MirDsp.h"

#include <catch2/catch.hpp>

namespace MIR
{
TEST_CASE("GetClusters")
{
   SECTION("minimal case")
   {
      std::vector<float> data { 2, 3 };
      const auto clusters = GetClusters(data);
      REQUIRE(clusters == std::vector<Cluster> { { 0u }, { 1u } });
   }

   SECTION("minimal case unsorted")
   {
      std::vector<float> data { 3, 2 };
      const auto clusters = GetClusters(data);
      REQUIRE(clusters == std::vector<Cluster> { { 1u }, { 0u } });
   }

   SECTION("with three data points")
   {
      std::vector<float> data { 2, 3, 5 };
      const auto clusters = GetClusters(data);
      REQUIRE(clusters == std::vector<Cluster> { { 0u }, { 1u }, { 2u } });
   }

   SECTION("all data points are equal")
   {
      std::vector<float> data { 2, 2, 2 };
      const auto clusters = GetClusters(data);
      REQUIRE(clusters == std::vector<Cluster> { { 0u, 1u, 2u } });
   }
}
} // namespace MIR
