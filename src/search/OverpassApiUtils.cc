#include "OverpassApiUtils.h"

#include "../utils/JsonUtils.h"
#include "../utils/WebClient.h"
#include "ProtoTypes.h"

#include <absl/log/log.h>
#include <rapidjson/document.h>

#include <format>

namespace
{

using namespace geo;

// Overpass API query format to find relations by name or English name.
constexpr const char* sz_requestByNameFormat =  //
   "[out:json];"
   "rel[\"name\"=\"{0}\"][\"boundary\"=\"administrative\"];"
   "out ids;";  // Return ids.

constexpr const char* sz_requestCityDetailsFormatj =
   "[out:json];"
   "relation({0});"
   "map_to_area->.a;"
   "("
   "  node[\"tourism\"=\"museum\"](area.a);"
   "  node[\"tourism\"=\"hotel\"](area.a);"
   ");"
   "out center tags;";

// Overpass API query format to find relations by coordinates.
constexpr const char* sz_requestByCoordinatesFormat =
   "[out:json];"
   "is_in({},{}) -> .areas;"  // Save "area" entities which contain a point with the given coordinates to .areas set.
   "("
   "rel(pivot.areas)[\"boundary\"=\"administrative\"];"
   "rel(pivot.areas)[\"place\"~\"^(city|town|state)$\"];"
   ");"         // Save "relation" entities with administrative boundary type or with city|town|state place
                // which define the outlines of the found "area" entities to the result set.
   "out ids;";  // Return ids.

}  // namespace

namespace geo::overpass
{

OsmIds ExtractRelationIds(const std::string& json)
{
   if (json.empty())
      return {};

   rapidjson::Document document;
   document.Parse(json.c_str());
   if (!document.IsObject())
      return {};

   OsmIds result;
   for (const auto& e : document["elements"].GetArray())
   {
      const auto& id = json::Get(e, "id");
      if (!id.IsNull() && json::GetString(json::Get(e, "type")) == "relation")
         result.emplace_back(json::GetInt64(id));
   }
   return result;
}

void AddFeaturesFromOverpass(const std::string& json, GeoProtoPlace& city)
{
   if (json.empty())
      return;

   rapidjson::Document document;
   document.Parse(json.c_str());
   if (!document.IsObject() || !document.HasMember("elements"))
      return;

   auto isAccommodationTourism = [](std::string_view v)
   {
      return v == "hotel" || v == "guest_house" || v == "hostel" || v == "apartment" || v == "motel" || v == "chalet" ||
             v == "alpine_hut";
   };

   auto isAccommodationAmenity = [](std::string_view v)
   {
      return v == "hotel" || v == "guest_house" || v == "hostel" || v == "apartment" || v == "bed_and_breakfast";
   };

   for (const auto& e : document["elements"].GetArray())
   {
      if (!json::Has(e, "type"))
         continue;
      std::string_view type = json::GetString(json::Get(e, "type"));
      if (type != "node" && type != "way" && type != "relation")
         continue;

      // Координаты
      double lat = 0.0;
      double lon = 0.0;
      if (type == "node")
      {
         if (!json::Has(e, "lat") || !json::Has(e, "lon"))
            continue;
         lat = e["lat"].GetDouble();
         lon = e["lon"].GetDouble();
      }
      else
      {
         if (!json::Has(e, "center"))
            continue;
         const auto& center = json::Get(e, "center");
         if (!json::Has(center, "lat") || !json::Has(center, "lon"))
            continue;
         lat = center["lat"].GetDouble();
         lon = center["lon"].GetDouble();
      }

      // Теги
      if (!json::Has(e, "tags"))
         continue;
      const auto& tags = json::Get(e, "tags");

      std::string_view tourism = "";
      std::string_view amenity = "";

      if (json::Has(tags, "tourism"))
         tourism = json::GetString(json::Get(tags, "tourism"));
      if (json::Has(tags, "amenity"))
         amenity = json::GetString(json::Get(tags, "amenity"));

      // Нормализация в два типа: museum или hotel
      std::string feature_type;
      if (tourism == "museum")
      {
         feature_type = "museum";
      }
      else if (isAccommodationTourism(tourism) || isAccommodationAmenity(amenity))
      {
         feature_type = "hotel";
      }
      else
      {
         continue;
      }

      auto* feature = city.add_features();
      feature->mutable_position()->set_latitude(lat);
      feature->mutable_position()->set_longitude(lon);

      // Важно: DebugHelpers печатает по tourism, поэтому кладём нормализованное значение.
      (*feature->mutable_tags())["tourism"] = feature_type;

      if (json::Has(tags, "name"))
         (*feature->mutable_tags())["name"] = std::string(json::GetString(json::Get(tags, "name")));

      if (json::Has(tags, "name:en"))
         (*feature->mutable_tags())["name:en"] = std::string(json::GetString(json::Get(tags, "name:en")));

      LOG(INFO) << "AddFeaturesFromOverpass: tourism=" << feature_type
                << ", name=" << (json::Has(tags, "name") ? json::GetString(json::Get(tags, "name")) : "")
                << ", lat=" << lat << ", lon=" << lon;
   }
}

void LoadFeaturesByRelationIds(WebClient& client, const overpass::OsmId& relationId, GeoProtoPlace& city)
{
   const std::string request = std::format(sz_requestCityDetailsFormatj, relationId);
   const std::string response = client.Post(request);
   return AddFeaturesFromOverpass(response, city);
}

OsmIds LoadRelationIdsByName(WebClient& client, const std::string& name)
{
   const std::string request = std::format(sz_requestByNameFormat, name);
   const std::string response = client.Post(request);
   return ExtractRelationIds(response);
}

OsmIds LoadRelationIdsByLocation(WebClient& client, double latitude, double longitude)
{
   const std::string request = std::format(sz_requestByCoordinatesFormat, latitude, longitude);
   const std::string response = client.Post(request);
   return ExtractRelationIds(response);
}

}  // namespace geo::overpass