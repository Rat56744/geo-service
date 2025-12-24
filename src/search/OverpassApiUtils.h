#pragma once

#include "ProtoTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace geo
{
class WebClient;
}  // namespace geo

namespace geo::overpass
{

using OsmId = std::int64_t;         // Type alias for OpenStreetMap (OSM) IDs.
using OsmIds = std::vector<OsmId>;  // Type alias for a list of OSM IDs.

// Extracts all IDs of entities with type "relation" from a JSON response.
// @param json: The JSON response from the Overpass API.
// @return: A list of OSM IDs for the relations found.
OsmIds ExtractRelationIds(const std::string& json);

void AddFeaturesFromOverpass(const std::string& json, GeoProtoPlace& city);

void LoadFeaturesByRelationIds(WebClient& client, const overpass::OsmId& relationId, GeoProtoPlace& city);

// Finds relation IDs by name using the Overpass API.
// @param client: WebClient instance to interact with the Overpass API.
// @param name: The name to search for.
// @return: A list of OSM IDs for the relations found.
OsmIds LoadRelationIdsByName(WebClient& client, const std::string& name);

// Finds relation IDs by location (latitude/longitude) using the Overpass API.
// @param client: WebClient instance to interact with the Overpass API.
// @param latitude: The latitude of the location.
// @param longitude: The longitude of the location.
// @return: A list of OSM IDs for the relations found.
OsmIds LoadRelationIdsByLocation(WebClient& client, double latitude, double longitude);

}  // namespace geo::overpass
