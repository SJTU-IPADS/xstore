#pragma once

#include <memory>

namespace fstore {

namespace datastream {

/*!
  Iterate all values in the stream
*/
template <typename KeyType, typename ValType>
class StreamIterator {
 public:
  virtual void begin() = 0;
  virtual bool valid() = 0;
  virtual void next()  = 0;
  virtual KeyType key() = 0;
  virtual ValType value() = 0;
  virtual ~StreamIterator() {
  }
};

template <typename KeyType, typename ValType>
class Stream {
 public:
  /*!
    \sa put
    \param key to the data
    \param val corresponding to the value
  */
  virtual bool put(const KeyType &key,const ValType &val) = 0;

  /*!
    \sa close the stream. Cannot issue any futher operation to it.
  */
  virtual void close() = 0;

  typedef std::unique_ptr<StreamIterator<KeyType, ValType>> iter_p_t;
  virtual iter_p_t get_iter() = 0;
};

}

} // end namespace
