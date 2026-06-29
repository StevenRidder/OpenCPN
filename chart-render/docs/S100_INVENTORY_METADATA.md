# S-100 Inventory Metadata Mapping

Status: FORMAT-3 schema note and fixture metadata examples

This note maps the public ArcGIS S-100 Hub inventory pattern into the Vulkan
portable nautical package schema. It is package metadata only: it is not a
VSG/GPU cache contract, not a renderer hot path, and not a portal UI.

The observed inventory pattern is a polygon feature layer named
`S100ProductInventory`. Its rows carry:

- `product_identifier`: short product family code such as `S-101` or `S-102`,
  with coded-value labels such as `INT.IHO.S-101.2.0`;
- `dataset_name`;
- `dataset_edition`;
- `dataset_reference_date`;
- `producer_code`;
- `description`;
- `source_link`;
- `service_link`;
- `content_link_1`, `content_link_2`, and `content_link_3`;
- polygon coverage geometry and service/item object ids.

The inventory is a discovery/catalog layer. A portable package may use these
fields to describe source lineage and coverage, but source conversion still owns
the dataset download/inspection, checksums, feature normalization, diagnostics,
and provenance.
If the current C++ source-kind enum does not yet contain a family-specific kind
such as S-102 bathymetric surface, the converter should preserve the product
family in metadata and add the enum extension deliberately in the converter
slice, not overload an ENC kind.

## Source Mapping

| ArcGIS inventory field | Portable package location | Notes |
|---|---|---|
| `product_identifier` code | `source.metadata["s100.product_family"]` | Store the short code, for example `S-101`. |
| Product-spec label/domain | `manifest.profile`, `source.metadata["s100.product_identifier"]` | Store the full identifier when available, for example `INT.IHO.S-101.2.0`. |
| `dataset_name` | `source.source_id`, `source.native_name`, `source.metadata["s100.dataset_name"]` | Use a stable package source id such as `s100:S-101:101US003CT1AA`. |
| `dataset_edition` | `source.edition`, `source.metadata["s100.dataset_edition"]` | If blank, emit a diagnostic instead of fabricating an edition. |
| `dataset_reference_date` | `source.metadata["s100.dataset_reference_date"]`, `manifest.source_epoch` candidate | For time-varying products, combine with validity fields below. |
| `producer_code` | `source.metadata["s100.producer_code"]`, provenance producer field | Preserve the producer code even when the producer name is only available from an item page. |
| `source_link` | `source.metadata["s100.source_link"]` | Portal/source identity only. Do not treat it as a renderer input. |
| `service_link` | `source.metadata["s100.service_link"]` | A converter may fetch from it, but the renderer must consume the portable package. |
| `content_link_1..3` | `source.metadata["s100.content_link.N"]` | Preserve all non-empty links in original order. |
| Inventory object id | provenance and trace handles | Use it to tie package records back to the catalog row. |
| Coverage polygon | `coverage.geometry`, `source.geographic_bbox` | Store normalized WGS84 coordinates in the package. |
| Coverage bbox | `coverage.metadata["bbox"]` or derived index | Keep the polygon as truth; bbox is an index/summary. |
| Product/schema edition | `manifest.metadata`, `source.metadata` | Split package schema version from S-100 product edition. |
| Checksums | package `checksums`, source metadata | If inventory lacks a content checksum, record `checksum_status=missing_from_inventory`. |

## Trace Handles

The portable package should retain a trace handle that survives conversion:

```text
s100_inventory:<service_item_id>:layer:<layer_id>:object:<object_id>
```

For a row from the observed inventory service this becomes:

```text
s100_inventory:4c38e4d4bdc44e9192ad5335a3f22590:layer:1:object:<OBJECTID>
```

Derived records then use stable package ids:

```text
source_id      = s100:<product_family>:<dataset_name>
coverage_id    = coverage:<product_family>:<dataset_name>
provenance_id  = prov:inventory:<product_family>:<dataset_name>
```

Do not use service URLs as source ids. URLs can move; the dataset name,
producer, product identifier, object id, and checksum/provenance bundle should
carry identity.

## Validity And Time Dimensions

Static chart products such as S-101 may only need `dataset_reference_date` and
edition/update metadata. Time-varying products such as S-111 surface currents
or future weather/wave products should reserve explicit package metadata:

```json
{
  "time_dimension": {
    "reference_time": "2026-03-17T00:00:00Z",
    "valid_time_start": "2026-03-17T00:00:00Z",
    "valid_time_end": "2026-03-17T06:00:00Z",
    "interval": "PT1H",
    "time_basis": "forecast_or_observation",
    "source_epoch": "producer:US00/reference:2026-03-17/valid:2026-03-17T00:00:00Z"
  }
}
```

Rules for time-aware products:

- `manifest.source_epoch` must include the validity slice, not just the dataset
  edition.
- coverage ids may be stable across time, but feature/raster/grid record ids
  must include the time slice if values change.
- missing validity metadata is a warning for static products and an error for
  time-varying products.
- environmental field textures or samples are downstream derived artifacts; the
  portable package stores the source data lineage and normalized values only.

## Validation Rules

A package built from an S-100 inventory row is invalid if:

- product family, dataset name, producer code, or coverage geometry is missing;
- the row product family and full product identifier disagree;
- source/service/content links are promoted to renderer or backend fields;
- S-100 product edition is confused with package schema version;
- coverage polygon coordinates are not normalized to WGS84 package coordinates;
- checksums are absent without an explicit diagnostic;
- time-varying products omit validity metadata;
- trace handles are missing from source, coverage, and provenance records.

It may be valid but incomplete when inventory fields are blank and diagnostics
make the gap explicit. That is common in demo or pre-operational inventory rows.

## Fixture Example: S-101 ENC Metadata

This example combines the public S-100 Hub inventory row for `101US003CT1AA`
with the corresponding ArcGIS item metadata, which includes the S-101 product
identifier, product edition, dataset reference date, and dataset edition.

```json
{
  "manifest": {
    "package_id": "s100:S-101:101US003CT1AA",
    "schema_version": 1,
    "profile": "s101",
    "source_epoch": "producer:US00/dataset:101US003CT1AA/edition:1.0/reference:2026-03-17",
    "metadata": {
      "s100.product_family": "S-101",
      "s100.product_identifier": "INT.IHO.S-101.2.0",
      "s100.product_edition": "2.0",
      "s100.encoding_specification": "S-100 Part 10a",
      "s100.encoding_specification_edition": "5.2",
      "distribution_notice": "NOT FOR NAVIGATION USE"
    }
  },
  "sources": [
    {
      "source_id": "s100:S-101:101US003CT1AA",
      "kind": "s101_dataset",
      "native_name": "101US003CT1AA",
      "edition": "1.0",
      "update": "",
      "content_hash": "",
      "checksum_status": "missing_from_inventory",
      "geographic_bbox": [-73.2, 40.8, -72.0, 42.0],
      "metadata": {
        "s100.dataset_name": "101US003CT1AA",
        "s100.dataset_reference_date": "2026-03-17",
        "s100.producer_code": "US00",
        "s100.producer_name": "Office of Coast Survey, National Ocean Service, NOAA",
        "s100.source_link": "https://esriho.maps.arcgis.com/home/item.html?id=7d596080e71241be851dba08426800d9",
        "s100.service_link": "https://mardev1.esri.com/server/rest/services/S100/101US003CT1AA/MapServer"
      }
    }
  ],
  "coverage": [
    {
      "coverage_id": "coverage:S-101:101US003CT1AA",
      "source_id": "s100:S-101:101US003CT1AA",
      "geometry": {
        "coordinate_reference": "EPSG:4326",
        "rings": [[[-73.2, 40.8], [-73.2, 42.0], [-72.0, 42.0], [-72.0, 40.8], [-73.2, 40.8]]]
      },
      "metadata": {
        "chart_family": "S-101",
        "boundary_policy": "catalog_coverage_polygon",
        "trace_handle": "s100_inventory:4c38e4d4bdc44e9192ad5335a3f22590:layer:1:object:<OBJECTID>"
      }
    }
  ],
  "provenance": [
    {
      "provenance_id": "prov:inventory:S-101:101US003CT1AA",
      "source_chart_id": "s100:S-101:101US003CT1AA",
      "source_object_id": "101US003CT1AA",
      "source_object_class": "S100ProductInventory",
      "conversion_stage": "s100_inventory_ingest",
      "transform_chain": ["arcgis_inventory_row", "coverage_polygon_to_wgs84", "portable_package_metadata"]
    }
  ],
  "checksums": {
    "manifest": "<computed-by-package-writer>",
    "sources": "<computed-by-package-writer>",
    "coverage": "<computed-by-package-writer>",
    "provenance": "<computed-by-package-writer>"
  },
  "diagnostics": [
    {
      "severity": "warning",
      "code": "s100_inventory_missing_content_checksum",
      "message": "Inventory row did not provide a source content checksum; converter must compute one before accepting source payload."
    }
  ]
}
```

## Fixture Example: S-102 Bathymetric Surface Metadata

This non-chart example uses an observed S-102 inventory row. The row exposes an
ImageServer service and coverage polygon but leaves dataset edition and
reference date blank, so the fixture keeps those blanks visible as diagnostics.

```json
{
  "manifest": {
    "package_id": "s100:S-102:102US004NY1AQ2522H7",
    "schema_version": 1,
    "profile": "s102",
    "source_epoch": "producer:US00/dataset:102US004NY1AQ2522H7/edition:unknown/reference:unknown",
    "metadata": {
      "s100.product_family": "S-102",
      "s100.product_identifier": "INT.IHO.S-102.3.0",
      "s100.product_edition": "3.0",
      "package_role": "portable_bathymetric_surface_metadata"
    }
  },
  "sources": [
    {
      "source_id": "s100:S-102:102US004NY1AQ2522H7",
      "kind": "bathymetric_surface",
      "native_name": "102US004NY1AQ2522H7",
      "edition": "",
      "update": "",
      "content_hash": "",
      "checksum_status": "missing_from_inventory",
      "geographic_bbox": [-74.1045946772, 40.4970313260, -73.7944421593, 40.8030285738],
      "metadata": {
        "s100.dataset_name": "102US004NY1AQ2522H7",
        "s100.dataset_edition": "",
        "s100.dataset_reference_date": "",
        "s100.producer_code": "US00",
        "s100.source_link": "https://esriho.maps.arcgis.com/home/item.html?id=7d554947acfa45c38c733404d2a1614e",
        "s100.service_link": "https://mardev1.esri.com/server/rest/services/S100/102US004NY1AQ2522H7/ImageServer"
      }
    }
  ],
  "coverage": [
    {
      "coverage_id": "coverage:S-102:102US004NY1AQ2522H7",
      "source_id": "s100:S-102:102US004NY1AQ2522H7",
      "geometry": {
        "coordinate_reference": "EPSG:4326",
        "rings": [[
          [-73.7998869697, 40.4970313260],
          [-74.1045946772, 40.4997941127],
          [-74.1005317692, 40.8030285738],
          [-73.7944421593, 40.8002362414],
          [-73.7998869697, 40.4970313260]
        ]]
      },
      "metadata": {
        "chart_family": "S-102",
        "boundary_policy": "catalog_coverage_polygon",
        "surface_semantics": "bathymetry_grid",
        "trace_handle": "s100_inventory:4c38e4d4bdc44e9192ad5335a3f22590:layer:1:object:<OBJECTID>"
      }
    }
  ],
  "diagnostics": [
    {
      "severity": "warning",
      "code": "s100_inventory_missing_dataset_edition",
      "message": "S-102 inventory row left dataset_edition blank."
    },
    {
      "severity": "warning",
      "code": "s100_inventory_missing_dataset_reference_date",
      "message": "S-102 inventory row left dataset_reference_date blank; source_epoch is incomplete until converter inspection."
    },
    {
      "severity": "warning",
      "code": "s100_inventory_missing_content_checksum",
      "message": "Inventory row did not provide a source content checksum; converter must compute one before accepting source payload."
    }
  ]
}
```

## Source Links Used

- ArcGIS S-100 Hub: `https://s100hub-esriho.hub.arcgis.com/`
- ArcGIS inventory item: `https://www.arcgis.com/home/item.html?id=4c38e4d4bdc44e9192ad5335a3f22590`
- ArcGIS inventory feature layer: `https://services.arcgis.com/9WwUuY7dz0Ulzqa5/arcgis/rest/services/Inventory_of_Maritime_Services/FeatureServer/1`
- ArcGIS S-101 item example: `https://www.arcgis.com/home/item.html?id=7d596080e71241be851dba08426800d9`
- IHO S-100 overview: `https://iho.int/en/s-100-universal-hydrographic-data-model`
