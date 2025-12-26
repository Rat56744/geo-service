#include "DebugHelpers.h"

#include "ProtoTypes.h"
#include "search/SearchEngine.h"
#include "search/SearchEngineItf.h"
#include "utils/ConfigConstants.h"
#include "utils/Configuration.h"
#include "utils/WebClient.h"

#include <absl/log/log.h>

#include <format>
#include <string>

namespace geo::debug
{

namespace
{

void printDetails(const GeoProtoPlaces& places)
{
   for (const auto& c : places)
   {
      LOG(INFO) << std::format("Found place {} ({}), country {} ({}), ({},{})", c.name(), c.name_en(), c.country(),
         c.country_en(), c.center().latitude(), c.center().longitude());

      for (const auto& o : c.features())
      {
         LOG(INFO) << std::format("   [{}] {} ({}) ({},{})", o.tags().at("tourism"),
            (o.tags().contains("name") ? o.tags().at("name") : ""),
            (o.tags().contains("name:en") ? o.tags().at("name:en") : ""), o.position().latitude(),
            o.position().longitude());
      }
   }
}

void printDetails(const WeatherInfoVector& weather)
{
   for (const auto& entry : weather)
   {
      LOG(INFO) << std::format("{:%F}: T min {}, T max {}, T avg {}", entry.time, entry.temperatureMin,
         entry.temperatureMax, entry.temperatureAverage);
   }
}

}  // namespace

void Search(const std::string& name, const std::string& configFilePath)
{
   Configuration configuration(configFilePath.c_str());
   geo::WebClient overpassApiClient(configuration.GetString(sz_overpassEndpointKey));
   geo::WebClient nominatimApiClient(configuration.GetString(sz_nominatimEndpointKey));
   geo::SearchEngine engine(overpassApiClient, nominatimApiClient);
   auto cities = engine.FindCitiesByName(name, true);
   printDetails(cities);
}

void Search(double latitude, double longitude, const std::string& configFilePath)
{
   Configuration configuration(configFilePath.c_str());
   geo::WebClient overpassApiClient(configuration.GetString(sz_overpassEndpointKey));
   geo::WebClient nominatimApiClient(configuration.GetString(sz_nominatimEndpointKey));
   geo::SearchEngine engine(overpassApiClient, nominatimApiClient);
   auto cities = engine.FindCitiesByPosition(latitude, longitude, true);
   printDetails(cities);
}

void Search(
   double latitude, double longitude, std::uint32_t rangeKm, std::uint32_t filter, const std::string& configFilePath)
{
   geo::ISearchEngine::RegionPreferences::Properties props = {
      {"minPeakHeight", "1000"}
   };
   std::uint32_t rangeM = rangeKm * 1000;
   Configuration configuration(configFilePath.c_str());
   geo::WebClient overpassApiClient(configuration.GetString(sz_overpassEndpointKey));
   geo::WebClient nominatimApiClient(configuration.GetString(sz_nominatimEndpointKey));
   geo::SearchEngine engine(overpassApiClient, nominatimApiClient);
   auto handler = engine.StartFindRegions();

   GeoProtoPlaces regions;
   auto maxBoxWidth = configuration.GetInt64(sz_maxBoxWidthKey);
   auto maxBoxHeight = configuration.GetInt64(sz_maxBoxHeightKey);
   for (auto& bbox : CreateBoundingBoxes(latitude, longitude, rangeM, maxBoxWidth, maxBoxHeight))
   {
      auto iterationResult = handler(bbox, {filter, props});
      regions.insert(regions.begin(), iterationResult.begin(), iterationResult.end());
   }
   printDetails(regions);
}

void RequestWeather(double latitude, double longitude, const std::string& fromDate, const std::string& toDate,
   const std::string& configFilePath)
{
   Configuration configuration(configFilePath.c_str());
   geo::WebClient overpassApiClient(configuration.GetString(sz_overpassEndpointKey));
   geo::WebClient nominatimApiClient(configuration.GetString(sz_nominatimEndpointKey));
   geo::SearchEngine engine(overpassApiClient, nominatimApiClient);

   const auto weather = engine.GetWeather(latitude, longitude, {StringToDate(fromDate), StringToDate(toDate)});
   printDetails(weather);
}

}  // namespace geo::debug
