#pragma once

#include <string>
#include <utility>

namespace polygon_codec {

// Status 用于表达“成功/失败”。
// - ok()==true 表示成功
// - ok()==false 表示失败，message() 提供失败原因
class Status {
 public:
  static Status Ok() { return Status(true, ""); }
  static Status Error(std::string msg) { return Status(false, std::move(msg)); }

  bool ok() const { return ok_; }
  const std::string& message() const { return message_; }

 private:
  Status(bool ok, std::string msg) : ok_(ok), message_(std::move(msg)) {}

  bool ok_ = true;
  std::string message_;
};

// StatusOr<T> 用于在返回值中同时携带“状态 + 数据”。
// - ok()==true 时，value() 有效
// - ok()==false 时，status() 提供错误原因
template <typename T>
class StatusOr {
 public:
  StatusOr(Status s) : status_(std::move(s)) {}
  StatusOr(T v) : status_(Status::Ok()), value_(std::move(v)), has_value_(true) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }

  const T& value() const { return value_; }
  T& value() { return value_; }

 private:
  Status status_;
  T value_{};
  bool has_value_ = false;
};

}  // namespace polygon_codec

