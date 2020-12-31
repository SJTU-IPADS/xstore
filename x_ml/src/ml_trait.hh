#pragma once

#include <string>
#include <vector>

#include "../../lib.hh"
#include "../../xutils/marshal.hh"

namespace xstore {

namespace xml {

/*!
  An ML trait abstracts a machine learning model,
  which executes `predicts` and `train`.
  It also abstracts away methods for serializing so that we can
  pass this model to different processes (possibly on a different machine).
 */
template<class Derived, typename Key>
class MLTrait
{
public:
  auto predict(const Key& key) -> double
  {
    return reinterpret_cast<Derived*>(this)->predict_impl(key);
  }

  void train(std::vector<Key>& train_data,
             std::vector<u64>& train_label,
             int step = 1)
  {
    return reinterpret_cast<Derived*>(this)->train_impl(
      train_data, train_label, step);
  }

  auto serialize() -> std::string
  {
    return reinterpret_cast<Derived*>(this)->serialize_impl();
  }

  /*!
    \ret: return serialized file name
   */
  auto serialize_to_file(const std::string& name) -> std::string
  {
    return reinterpret_cast<Derived*>(this)->serialize_to_file_impl(name);
  }

  /*!
    init myself from `data` returned from serialize.
   */
  void from_serialize(const ::xstore::string_view& data)
  {
    return reinterpret_cast<Derived*>(this)->from_serialize_impl(data);
  }

  /*!
  load the model from the file
  */
  void from_file(const std::string& file_name)
  {
    return reinterpret_cast<Derived*>(this)->from_file_view(file_name);
  }

  void from_file_view(const ::xstore::string_view& file_name)
  {
    reinterpret_cast<Derived*>(this)->from_file_impl(file_name);
  }
};

} // namespace xml

} // namespace xstore
