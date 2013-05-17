#ifndef RESTFUL_MAPPER_MAPPER_H
#define RESTFUL_MAPPER_MAPPER_H

#include <restful_mapper/field.h>
#include <restful_mapper/json.h>
#include <restful_mapper/relation.h>

namespace restful_mapper
{

enum MapperConfig
{
  IGNORE_MISSING_FIELDS = 1,  // Ignore missing fields in input
  INCLUDE_PRIMARY_KEY   = 2,  // Include primary key in output, if it is not null
  IGNORE_DIRTY_FLAG     = 4,  // Include non-dirty fields in output
  TOUCH_FIELDS          = 8,  // Touch fields after reading input
  KEEP_FIELDS_DIRTY     = 16, // Do not clean fields after outputting
  OUTPUT_SINGLE_FIELD   = 32  // Output only a single field (set in field_filter_)
};

class Mapper
{
public:
  Mapper(const int &flags = 0)
  {
    flags_ = flags;

    if (!should_output_single_field())
    {
      emitter_.emit_map_open();
    }
  }

  Mapper(std::string json_struct, const int &flags = 0)
  {
    flags_ = flags;

    if (!should_output_single_field())
    {
      emitter_.emit_map_open();
    }

    parser_.load(json_struct);
  }

  const int &flags() const
  {
    return flags_;
  }

  void set_flags(const int &flags)
  {
    flags_ = flags;
  }

  const std::string &field_filter() const
  {
    return field_filter_;
  }

  void set_field_filter(const std::string &field_filter)
  {
    field_filter_ = field_filter;
  }

  std::string dump()
  {
    if (!should_output_single_field())
    {
      emitter_.emit_map_close();
    }

    return emitter_.dump();
  }

  std::string get(const char *key) const
  {
    return parser_.find(key).dump();
  }

  void set(const char *key, std::string json_struct)
  {
    if (should_output_single_field() && field_filter_ != key) return;

    if (!should_output_single_field())
    {
      emitter_.emit(key);
    }

    emitter_.emit_json(json_struct);
  }

  template <class T> void get(const char *key, Field<T> &attr) const
  {
    if (parser_.exists(key))
    {
      Json::Node node = parser_.find(key);

      if (node.is_null())
      {
        attr.clear(true);
      }
      else
      {
        attr.set(node, true);
      }
    }
    else if (!should_ignore_missing_fields())
    {
      Json::not_found(key);
    }

    if (should_touch_fields())
    {
      attr.touch();
    }
  }

  template <class T> void set(const char *key, const Field<T> &attr)
  {
    if (should_output_single_field() && field_filter_ != key) return;
    if (!should_ignore_dirty_flag() && !attr.is_dirty()) return;

    if (!should_output_single_field())
    {
      emitter_.emit(key);
    }

    if (attr.is_null())
    {
      emitter_.emit_null();
    }
    else
    {
      emitter_.emit(attr);
    }

    if (!should_keep_fields_dirty()) attr.clean();
  }

  void get(const char *key, Field<std::time_t> &attr) const
  {
    if (parser_.exists(key))
    {
      Json::Node node = parser_.find(key);

      if (node.is_null())
      {
        attr.clear(true);
      }
      else
      {
        attr.set(node.to_string(), true);
      }
    }
    else if (!should_ignore_missing_fields())
    {
      Json::not_found(key);
    }

    if (should_touch_fields())
    {
      attr.touch();
    }
  }

  void set(const char *key, const Field<std::time_t> &attr)
  {
    if (should_output_single_field() && field_filter_ != key) return;
    if (!should_ignore_dirty_flag() && !attr.is_dirty()) return;

    if (!should_output_single_field())
    {
      emitter_.emit(key);
    }

    if (attr.is_null())
    {
      emitter_.emit_null();
    }
    else
    {
      emitter_.emit(attr.to_iso8601(true));
    }

    if (!should_keep_fields_dirty()) attr.clean();
  }

  void get(const char *key, Primary &attr) const
  {
    if (parser_.exists(key))
    {
      Json::Node node = parser_.find(key);

      if (node.is_null())
      {
        attr.clear(true);
      }
      else
      {
        attr.set(node.to_int(), true);
      }
    }
    else if (!should_ignore_missing_fields())
    {
      Json::not_found(key);
    }

    if (should_touch_fields())
    {
      attr.touch();
    }
  }

  void set(const char *key, const Primary &attr)
  {
    if (should_output_single_field() && field_filter_ != key) return;
    if (!should_include_primary_key() || attr.is_null()) return;

    if (!should_output_single_field())
    {
      emitter_.emit(key);
    }

    emitter_.emit(attr.get());

    if (!should_keep_fields_dirty()) attr.clean();
  }

  template <class T> void get(const char *key, HasOne<T> &attr) const
  {
    if (parser_.empty(key)) return;

    attr.from_json(get(key), flags_ | INCLUDE_PRIMARY_KEY);
  }

  template <class T> void set(const char *key, const HasOne<T> &attr)
  {
    if (should_output_single_field() && field_filter_ != key) return;
    if (!should_ignore_dirty_flag() && !attr.is_dirty()) return;

    set(key, attr.to_json((flags_ | INCLUDE_PRIMARY_KEY) & ~OUTPUT_SINGLE_FIELD));

    if (!should_keep_fields_dirty()) attr.clean();
  }

  template <class T> void get(const char *key, HasMany<T> &attr) const
  {
    if (parser_.empty(key)) return;

    attr.from_json(get(key), flags_ | INCLUDE_PRIMARY_KEY);
  }

  template <class T> void set(const char *key, const HasMany<T> &attr)
  {
    if (should_output_single_field() && field_filter_ != key) return;
    if (!should_ignore_dirty_flag() && !attr.is_dirty()) return;

    set(key, attr.to_json((flags_ | INCLUDE_PRIMARY_KEY) & ~OUTPUT_SINGLE_FIELD));

    if (!should_keep_fields_dirty()) attr.clean();
  }

private:
  Json::Emitter emitter_;
  Json::Parser parser_;

  int flags_;
  std::string field_filter_;

  inline bool should_ignore_missing_fields() const
  {
    return (flags_ & IGNORE_MISSING_FIELDS) == IGNORE_MISSING_FIELDS;
  }

  inline bool should_include_primary_key() const
  {
    return (flags_ & INCLUDE_PRIMARY_KEY) == INCLUDE_PRIMARY_KEY;
  }

  inline bool should_ignore_dirty_flag() const
  {
    return (flags_ & IGNORE_DIRTY_FLAG) == IGNORE_DIRTY_FLAG;
  }

  inline bool should_touch_fields() const
  {
    return (flags_ & TOUCH_FIELDS) == TOUCH_FIELDS;
  }

  inline bool should_keep_fields_dirty() const
  {
    return (flags_ & KEEP_FIELDS_DIRTY) == KEEP_FIELDS_DIRTY;
  }

  inline bool should_output_single_field() const
  {
    return (flags_ & OUTPUT_SINGLE_FIELD) == OUTPUT_SINGLE_FIELD;
  }
};

}

#endif // RESTFUL_MAPPER_MAPPER_H
