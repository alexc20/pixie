#pragma once

#include <vector>

#include "src/carnot/udf/base.h"
#include "src/carnot/udfspb/udfs.pb.h"

namespace pl {
namespace carnot {
namespace udf {

class AnyUDTF {
 public:
  virtual ~AnyUDTF() = default;
};

// Forward declaration for UDTF since there is a circular dependency with some code in this file.
template <typename Derived>
class UDTF;

/**
 * UDTFArg contains argument information for UDTFs. These are input parameters of the UDTF.
 *
 * This class is compile time constant.
 */
class UDTFArg {
 public:
  constexpr UDTFArg() = delete;
  constexpr UDTFArg(std::string_view name, types::DataType type, std::string_view desc)
      : UDTFArg(name, type, types::SemanticType::ST_UNSPECIFIED, desc) {}

  constexpr UDTFArg(std::string_view name, types::DataType type, types::SemanticType stype,
                    std::string_view desc)
      : name_(name), type_(type), stype_(stype), desc_(desc) {
    for (auto c : name) {
      COMPILE_TIME_ASSERT(c != ' ', "Col name can't contain spaces");
    }
    COMPILE_TIME_ASSERT(type != types::DataType::DATA_TYPE_UNKNOWN, "Col type cannot be unknown");
    COMPILE_TIME_ASSERT(desc.size() != 0, "Description must be specified");
  }

  constexpr const std::string_view name() const { return name_; }
  constexpr types::DataType type() const { return type_; }
  constexpr types::SemanticType stype() const { return stype_; }
  constexpr const std::string_view desc() const { return desc_; }

 private:
  std::string_view name_;
  types::DataType type_;
  types::SemanticType stype_;
  std::string_view desc_;
};

/**
 * ColInfo contains information about one output column of an UDTF.
 *
 * This class is compile time constant.
 */
class ColInfo {
 public:
  constexpr ColInfo() = delete;
  /**
   * Create a new ColInfo. All arguments must be compile time static.
   * @param name The name of the column. No spaces allowed.
   * @param type The data type of the column.
   * @param ptype The pattern type of the column.
   * @param desc The description of the column.
   */
  constexpr ColInfo(std::string_view name, types::DataType type, types::PatternType ptype,
                    std::string_view desc)
      : name_(name), type_(type), ptype_(ptype), desc_(desc) {
    for (auto c : name) {
      COMPILE_TIME_ASSERT(c != ' ', "Col name can't contain spaces");
    }
    COMPILE_TIME_ASSERT(type != types::DataType::DATA_TYPE_UNKNOWN, "Col type cannot be unknown");
    COMPILE_TIME_ASSERT(ptype != types::PatternType::UNSPECIFIED, "Pattern type must be specified");
    COMPILE_TIME_ASSERT(desc.size() != 0, "Description must be specified");
  }

  constexpr const std::string_view name() const { return name_; }
  constexpr types::DataType type() const { return type_; }
  constexpr types::PatternType ptype() const { return ptype_; }
  constexpr const std::string_view desc() const { return desc_; }

 protected:
  const std::string_view name_;
  const types::DataType type_;
  const types::PatternType ptype_;
  const std::string_view desc_;
};

template <typename T>
struct UDTFChecker;

/**
 * UDTFTraits allows access to compile time traits of a given UDTF.
 * @tparam TUDTF A class that derives from UDTF<T>.
 */
template <typename TUDTF>
class UDTFTraits {
 public:
  /**
   * Checks to see if an InitArgs() function exists.
   */
  static constexpr bool HasInitArgsFn() { return InitArgsFnHelper<TUDTF>::value; }

  /**
   * Checks to see if an Init(...) function exists.
   * @return
   */
  static constexpr bool HasInitFn() { return InitFnHelper<TUDTF>::value; }

  /**
   * Checks to see if InitArgs() is correct signature.
   */
  template <class Q = TUDTF>
  typename std::enable_if_t<UDTFTraits<Q>::HasInitArgsFn(),
                            bool> static constexpr HasCorrectInitArgsSignature() {
    return CorrectInitArgsTypeHelper<std::result_of_t<decltype (&TUDTF::InitArgs)()>>::value;
  }

  template <class Q = TUDTF>
  typename std::enable_if_t<!UDTFTraits<Q>::HasInitArgsFn(),
                            bool> static constexpr HasCorrectInitArgsSignature() {
    return false;
  }

  // Checks to make sure InitArgumentsTypes match the Init function.
  // Only valid if both functions exist.
  template <class Q = TUDTF>
  typename std::enable_if_t<UDTFTraits<Q>::HasInitFn() && UDTFTraits<Q>::HasInitArgsFn(),
                            bool> static constexpr HasConsistentInitArgs() {
    constexpr auto init_args_from_def = UDTFTraits<Q>::InitArgumentTypes();
    constexpr auto init_args_from_func = UDTFTraits<Q>::GetUDTFInitArgumentsFromFunc();

    if (init_args_from_def.size() != init_args_from_func.size()) {
      return false;
    }

    for (size_t i = 0; i < init_args_from_func.size(); ++i) {
      if (init_args_from_def[i] != init_args_from_func[i]) {
        return false;
      }
    }
    return true;
  }

  template <class Q = TUDTF>
  typename std::enable_if_t<!(UDTFTraits<Q>::HasInitFn() && UDTFTraits<Q>::HasInitArgsFn()),
                            bool> static constexpr HasConsistentInitArgs() {
    return false;
  }

  /**
   * Checks to see if OutputRelation() exists.
   */
  static constexpr bool HasOutputRelationFn() { return OutputRelationFnHelper<TUDTF>::value; }

  /**
   * Checks to see if OutputRelation() has correct signature.
   */
  static constexpr bool HasCorrectOutputRelationFnSignature() {
    return HasOutputRelationFn() &&
           CorrectOutputRelationTypeHelper<
               std::result_of_t<decltype (&TUDTF::OutputRelation)()>>::value;
  }

  /**
   * Checks to see if Executor() exists.
   */
  static constexpr bool HasExecutorFn() { return ExecutorFnHelper<TUDTF>::value; }

  /**
   * Checks to see if Executor() returns udfspb::UDTFSourceExecutor.
   * @return
   */
  static constexpr bool HasCorrectExectorFnReturnType() {
    return HasExecutorFn() && std::is_same_v<std::result_of_t<decltype (&TUDTF::Executor)()>,
                                             udfspb::UDTFSourceExecutor>;
  }

  /**
   * Checks to see if NextRecord() exists.
   * @return
   */
  static constexpr bool HasNextRecordFn() { return NextRecordFnHelper<TUDTF>::value; }

  /**
   * Gets the input arguments (compile time).
   * @return std::array of the init arguments.
   */
  static constexpr auto InitArgumentTypes() {
    constexpr auto initargs = TUDTF::InitArgs();
    return ArrayTransform(
        initargs, [](const UDTFArg& arg) -> auto { return arg.type(); });
  }

  /**
   * Gets the type of the output relations (compile time).
   * @return std::array of the output relation types.
   */
  static constexpr auto OutputRelationTypes() {
    constexpr auto relation = TUDTF::OutputRelation();
    return ArrayTransform(
        relation, [](const ColInfo& info) -> auto { return info.type(); });
  }

  /**
   * Gets the names of the output relation (compile time).
   * @return std::array<std::string_view> containing the output relation names.
   */
  static constexpr auto OutputRelationNames() {
    constexpr auto relation = TUDTF::OutputRelation();
    return ArrayTransform(
        relation, [](const ColInfo& info) -> auto { return info.name(); });
  }

 private:
  static constexpr auto GetUDTFInitArgumentsFromFunc() {
    return UDTFTraits::GetInitArgumentsTypeHelper(&TUDTF::Init);
  }

  template <typename T, typename... Types>
  static constexpr std::array<types::DataType, sizeof...(Types)> GetInitArgumentsTypeHelper(
      Status (T::*)(FunctionContext*, Types...)) {
    return std::array<types::DataType, sizeof...(Types)>(
        {types::ValueTypeTraits<Types>::data_type...});
  }

  /*************************************
   * Templates to check Init Args
   *************************************/
  template <typename T, typename = void>
  struct InitArgsFnHelper : std::false_type {};

  template <typename T>
  struct InitArgsFnHelper<T, std::void_t<decltype(&T::InitArgs)>> : std::true_type {};

  template <typename T, size_t = 0>
  struct CorrectInitArgsTypeHelper : std::false_type {};

  template <size_t N>
  struct CorrectInitArgsTypeHelper<std::array<UDTFArg, N>> : std::true_type {};

  template <typename T, typename = void>
  struct InitFnHelper : std::false_type {};

  template <typename T>
  struct InitFnHelper<T, std::void_t<decltype(&T::Init)>> : std::true_type {};

  /*************************************
   * Templates to check output relation
   *************************************/
  template <typename T, typename = void>
  struct OutputRelationFnHelper : std::false_type {};

  template <typename T>
  struct OutputRelationFnHelper<T, std::void_t<decltype(&T::OutputRelation)>> : std::true_type {};

  template <typename T, size_t = 0>
  struct CorrectOutputRelationTypeHelper : std::false_type {};

  template <size_t N>
  struct CorrectOutputRelationTypeHelper<std::array<ColInfo, N>> : std::true_type {};

  /*************************************
   * Templates to check Executor() func.
   *************************************/
  template <typename T, typename = void>
  struct ExecutorFnHelper : std::false_type {};

  template <typename T>
  struct ExecutorFnHelper<T, std::void_t<decltype(&T::Executor)>> : std::true_type {};

  template <typename T, typename = void>
  struct NextRecordFnHelper : std::false_type {};

  template <typename T>
  struct NextRecordFnHelper<
      T, std::void_t<decltype (&T::NextRecord)(FunctionContext*, typename T::RecordWriter*)>>
      : std::true_type {};
};

/**
 * RecordWriterProxy is used to write output records for the UDTF.
 * @tparam TUDTF The UDTF class.
 */
template <typename TUDTF>
class RecordWriterProxy final {
 public:
  explicit RecordWriterProxy(std::vector<arrow::ArrayBuilder*>* outputs) : outputs_(outputs) {
    CHECK(outputs != nullptr);
  }

  ~RecordWriterProxy() {
    // Check that all cols have the same length.
    CHECK(CheckCols());
  }

  /**
   * Append to the given column index.
   * Type checks based on the index provided.
   */
  template <size_t idx>
  inline void Append(
      typename types::DataTypeTraits<UDTFTraits<TUDTF>::OutputRelationTypes()[idx]>::value_type
          val) {
    DCHECK(idx < outputs_->size());
    DCHECK(ToArrowType(UDTFTraits<TUDTF>::OutputRelationTypes()[idx]) ==
           (*outputs_)[idx]->type()->id());
    AppendToBuilder(
        static_cast<typename types::DataTypeTraits<
            UDTFTraits<TUDTF>::OutputRelationTypes()[idx]>::arrow_builder_type*>((*outputs_)[idx]),
        val);
  }

  // Compile time function to get the index for a column with the specified name.
  static constexpr size_t ColIdx(std::string_view col_name) {
    constexpr auto col_names = UDTFTraits<TUDTF>::OutputRelationNames();
    size_t idx = 0;
    for (idx = 0; idx < col_names.size(); ++idx) {
      if (col_names[idx] == col_name) {
        return idx;
      }
    }

    COMPILE_TIME_ASSERT(idx >= col_names.size(), "Could not find key");
    return -1;
  }

 private:
  template <typename T, typename ValueType>
  void AppendToBuilder(T* builder, ValueType v) {
    DCHECK(builder->length() < builder->capacity());
    // If it's a string type we also need to allocate memory for the data.
    // This actually applies to all non-fixed data allocations.
    // PL_CARNOT_UPDATE_FOR_NEW_TYPES.
    if constexpr (std::is_same_v<arrow::StringBuilder, T>) {
      [[maybe_unused]] bool res = builder->ReserveData(v.size()).ok();
      DCHECK(res);
      builder->UnsafeAppend(v);
    } else {
      builder->UnsafeAppend(v.val);
    }
  }
  // Returns true if all cols have the same length.
  bool CheckCols() {
    if (outputs_->size() == 0) {
      return true;
    }

    int64_t s = (*outputs_)[0]->length();
    for (const auto& [idx, col] : Enumerate(*outputs_)) {
      if (col->length() != s) {
        LOG(ERROR) << absl::Substitute(
            "Column at idx=$0 has wrong number of records. Expected=$1, got=$2", idx, col->length(),
            s);
        return false;
      }
    }
    return true;
  }

  std::vector<arrow::ArrayBuilder*>* outputs_;
};

template <typename T>
struct UDTFChecker {
 private:
  using TR = UDTFTraits<T>;
  static_assert(std::is_base_of_v<UDTF<T>, T>, "UDTF must be derived from UDTF<T>");
  // Either both or None of InitArgs and Init must be specified.
  static_assert(!(TR::HasInitFn() ^ TR::HasInitArgsFn()),
                "Either both or none of InitArgs() and Init(...) must exist");

  // InitArgs must return std::array<UDTFArg, N>.
  static_assert(!TR::HasInitArgsFn() || TR::HasCorrectInitArgsSignature(),
                "Init args must return std::array<UDTFArg, N>");
  static_assert(!TR::HasInitFn() || TR::HasConsistentInitArgs(),
                "Specified init args should match init function");

  // Check OutputRelation().
  static_assert(TR::HasOutputRelationFn(), "Missing output relation func");
  static_assert(TR::HasCorrectOutputRelationFnSignature(),
                "Output relation function has incorrect signature");

  // Check that Executor exists and returns the executor type.
  static_assert(TR::HasExecutorFn(), "UDTF must have an Exectuor() func");
  static_assert(TR::HasCorrectExectorFnReturnType(), "Executor() must return UDTFSourceExecutor");
  // Check that NextRecord exists and is well formed.
  static_assert(
      TR::HasNextRecordFn(),
      "UDTF must have NextRecord func of form NextRecord(FunctionContext, RecordWriterProxy*)");
};

/**
 * UDTF<T> is the base class that all UDTFs need to derive from.
 * This class contains type dependent shared functions.
 *
 * Sample usage:
 *   class OutputsConstStringUDTF: public <OutputConstStringUDTF> {
 *    public:
 *     // Specify where this UDTF is executed.
 *     static constexpr auto Exector() {
 *       return udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS;
 *     }
 *
 *     static constexpr auto InitArgs() {
 *       return MakeArray(
 *         UDTFArg("outstr", types::DataType::STRING, "The value of the output string"),
 *         UDTFArg("count", types::DataType::INT64, "Number of time to output the string"));
 *     }
 *
 *     static constexpr auto OutputRelation() {
 *       return MakeArray(
 *          UDTFArg("out", types::DataType::STRING, types::PatternType::GENERAL, "string result"));
 *     }
 *
 *     Status Init(types::StringValue outstr, types::Int64Value count) {
 *         outstr_ = outstr;
 *         max_count_ = count.val;
 *     }
 *
 *     bool NextRecord(FunctionContext *, RecordWriter *rw) {
 *       rw->Append<IndexOf("out")>(outstr_);
 *       if (count == (max_count_ - 1)) {
 *         return false;
 *       }
 *       return true; // more records
 *     }
 *
 *    private:
 *     types::StringValue outstr_;
 *     int64_t max_count_ = 0;
 *     int64_t count_ = 0;
 *   }
 *
 * @tparam Derived The name of the derived class.
 */
template <typename Derived>
class UDTF : public AnyUDTF {
 public:
  using RecordWriter = RecordWriterProxy<Derived>;
  using Checker = UDTFChecker<Derived>;

  /**
   * Returns the index of the output column if it exists.
   * @param col The name of the column.
   * @return Index of the column (or compile time assert).
   */
  static constexpr size_t IndexOf(std::string_view col) { return RecordWriter::ColIdx(col); }
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
