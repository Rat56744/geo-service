#include "OverpassApiUtils.h"

#include "../utils/JsonUtils.h"
#include "../utils/WebClient.h"
#include "ProtoTypes.h"

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
   "map_to_area;"
   "(node[\"tourism\"=\"museum\"](area); node[\"tourism\"=\"hotel\"](area););"
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

   for (const auto& e : document["elements"].GetArray())
   {
      if (!json::Has(e, "type") || json::GetString(json::Get(e, "type")) != "node")
         continue;

      if (!json::Has(e, "tags"))
         continue;
      const auto& tags = json::Get(e, "tags");

      if (!json::Has(tags, "tourism"))
         continue;
      std::string_view tourism = json::GetString(json::Get(tags, "tourism"));
      if (tourism != "museum" && tourism != "hotel")
         continue;

      auto* feature = city.add_features();

      feature->mutable_position()->set_latitude(e["lat"].GetDouble());
      feature->mutable_position()->set_longitude(e["lon"].GetDouble());

      (*feature->mutable_tags())["tourism"] = std::string(tourism);

      if (json::Has(tags, "name"))
      {
         (*feature->mutable_tags())["name"] = std::string(json::GetString(json::Get(tags, "name")));
      }

      if (json::Has(tags, "name:en"))
      {
         (*feature->mutable_tags())["name:en"] = std::string(json::GetString(json::Get(tags, "name:en")));
      }
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
