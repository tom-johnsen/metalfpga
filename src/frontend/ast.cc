#include "frontend/ast.hh"

#include <algorithm>
#include <limits>
#include <utility>

namespace gpga {

namespace {

uint64_t MaskForWidth(int width) {
  if (width <= 0) {
    return 0ull;
  }
  if (width >= 64) {
    return 0xFFFFFFFFFFFFFFFFull;
  }
  return (1ull << width) - 1ull;
}

int MinimalWidth(uint64_t value) {
  if (value == 0) {
    return 1;
  }
  int width = 0;
  while (value > 0) {
    value >>= 1;
    ++width;
  }
  return width;
}

int ValueWidth(const FourStateValue& value) {
  return value.width > 0 ? value.width : 64;
}

bool ExprIsSigned(const Expr& expr) {
  switch (expr.kind) {
    case ExprKind::kIdentifier:
      return false;
    case ExprKind::kNumber:
      return expr.is_signed;
    case ExprKind::kString:
      return false;
    case ExprKind::kUnary:
      if (expr.unary_op == 'S') {
        return true;
      }
      if (expr.unary_op == 'U') {
        return false;
      }
      if (expr.unary_op == 'C') {
        return false;
      }
      return expr.operand ? ExprIsSigned(*expr.operand) : false;
    case ExprKind::kBinary: {
      bool lhs = expr.lhs ? ExprIsSigned(*expr.lhs) : false;
      bool rhs = expr.rhs ? ExprIsSigned(*expr.rhs) : false;
      return lhs && rhs;
    }
    case ExprKind::kTernary: {
      bool then_signed =
          expr.then_expr ? ExprIsSigned(*expr.then_expr) : false;
      bool else_signed =
          expr.else_expr ? ExprIsSigned(*expr.else_expr) : false;
      return then_signed && else_signed;
    }
    case ExprKind::kSelect:
    case ExprKind::kIndex:
      return expr.base ? ExprIsSigned(*expr.base) : false;
    case ExprKind::kCall:
      return false;
    case ExprKind::kConcat:
      return false;
  }
  return false;
}

int64_t SignedValue(uint64_t bits, int width) {
  if (width <= 0) {
    return 0;
  }
  if (width >= 64) {
    return static_cast<int64_t>(bits);
  }
  uint64_t mask = (1ull << width) - 1ull;
  bits &= mask;
  uint64_t sign = 1ull << (width - 1);
  if ((bits & sign) != 0) {
    uint64_t extended = bits | (~mask);
    return static_cast<int64_t>(extended);
  }
  return static_cast<int64_t>(bits);
}

FourStateValue ResizeValue(const FourStateValue& value, int width) {
  FourStateValue out = value;
  out.width = width;
  uint64_t mask = MaskForWidth(width);
  out.value_bits &= mask;
  out.x_bits &= mask;
  out.z_bits &= mask;
  return out;
}

FourStateValue NormalizeUnknown(const FourStateValue& value, int width) {
  FourStateValue out = ResizeValue(value, width);
  out.x_bits = (out.x_bits | out.z_bits) & MaskForWidth(width);
  out.z_bits = 0;
  return out;
}

FourStateValue MakeKnown(uint64_t value, int width) {
  FourStateValue out;
  out.width = width;
  out.value_bits = value & MaskForWidth(width);
  return out;
}

FourStateValue AllX(int width) {
  FourStateValue out;
  out.width = width;
  out.value_bits = 0;
  out.x_bits = MaskForWidth(width);
  out.z_bits = 0;
  return out;
}

FourStateValue MergeUnknown(const FourStateValue& a, const FourStateValue& b) {
  int width = std::max(ValueWidth(a), ValueWidth(b));
  FourStateValue left = NormalizeUnknown(a, width);
  FourStateValue right = NormalizeUnknown(b, width);
  uint64_t mask = MaskForWidth(width);
  uint64_t left_known = (~left.x_bits) & mask;
  uint64_t right_known = (~right.x_bits) & mask;
  uint64_t same = ~(left.value_bits ^ right.value_bits) & left_known &
                  right_known & mask;
  FourStateValue out;
  out.width = width;
  out.value_bits = left.value_bits & same;
  out.x_bits = mask & ~same;
  out.z_bits = 0;
  return out;
}

}  // namespace

std::unique_ptr<Expr> CloneExpr(const Expr& expr) {
  auto out = std::make_unique<Expr>();
  out->kind = expr.kind;
  out->ident = expr.ident;
  out->string_value = expr.string_value;
  out->number = expr.number;
  out->value_bits = expr.value_bits;
  out->x_bits = expr.x_bits;
  out->z_bits = expr.z_bits;
  out->number_width = expr.number_width;
  out->has_width = expr.has_width;
  out->has_base = expr.has_base;
  out->base_char = expr.base_char;
  out->is_signed = expr.is_signed;
  out->is_real_literal = expr.is_real_literal;
  out->op = expr.op;
  out->unary_op = expr.unary_op;
  out->msb = expr.msb;
  out->lsb = expr.lsb;
  out->has_range = expr.has_range;
  out->indexed_range = expr.indexed_range;
  out->indexed_desc = expr.indexed_desc;
  out->indexed_width = expr.indexed_width;
  out->repeat = expr.repeat;
  if (expr.operand) {
    out->operand = CloneExpr(*expr.operand);
  }
  if (expr.lhs) {
    out->lhs = CloneExpr(*expr.lhs);
  }
  if (expr.rhs) {
    out->rhs = CloneExpr(*expr.rhs);
  }
  if (expr.condition) {
    out->condition = CloneExpr(*expr.condition);
  }
  if (expr.then_expr) {
    out->then_expr = CloneExpr(*expr.then_expr);
  }
  if (expr.else_expr) {
    out->else_expr = CloneExpr(*expr.else_expr);
  }
  if (expr.base) {
    out->base = CloneExpr(*expr.base);
  }
  if (expr.index) {
    out->index = CloneExpr(*expr.index);
  }
  if (expr.msb_expr) {
    out->msb_expr = CloneExpr(*expr.msb_expr);
  }
  if (expr.lsb_expr) {
    out->lsb_expr = CloneExpr(*expr.lsb_expr);
  }
  if (expr.repeat_expr) {
    out->repeat_expr = CloneExpr(*expr.repeat_expr);
  }
  for (const auto& element : expr.elements) {
    out->elements.push_back(CloneExpr(*element));
  }
  for (const auto& arg : expr.call_args) {
    out->call_args.push_back(CloneExpr(*arg));
  }
  return out;
}

bool EvalConstExpr4State(const Expr& expr,
                         const std::unordered_map<std::string, int64_t>& params,
                         FourStateValue* out_value, std::string* error) {
  if (!out_value) {
    return false;
  }
  switch (expr.kind) {
    case ExprKind::kNumber: {
      int width = expr.has_width && expr.number_width > 0
                      ? expr.number_width
                      : std::max(32, std::max({MinimalWidth(expr.value_bits),
                                               MinimalWidth(expr.x_bits),
                                               MinimalWidth(expr.z_bits)}));
      FourStateValue out;
      out.width = width;
      out.value_bits = expr.value_bits;
      out.x_bits = expr.x_bits;
      out.z_bits = expr.z_bits;
      *out_value = ResizeValue(out, width);
      return true;
    }
    case ExprKind::kString:
      if (error) {
        *error = "string literal not allowed in constant expression";
      }
      return false;
    case ExprKind::kIdentifier: {
      auto it = params.find(expr.ident);
      if (it == params.end()) {
        if (error) {
          *error = "unknown parameter '" + expr.ident + "'";
        }
        return false;
      }
      int width = MinimalWidth(static_cast<uint64_t>(it->second));
      *out_value = MakeKnown(static_cast<uint64_t>(it->second), width);
      return true;
    }
    case ExprKind::kUnary: {
      FourStateValue value;
      if (!EvalConstExpr4State(*expr.operand, params, &value, error)) {
        return false;
      }
      int width = ValueWidth(value);
      FourStateValue normalized = NormalizeUnknown(value, width);
      switch (expr.unary_op) {
        case '+':
          if (normalized.HasXorZ()) {
            *out_value = AllX(width);
          } else {
            *out_value = NormalizeUnknown(value, width);
          }
          return true;
        case '-':
          if (normalized.HasXorZ()) {
            *out_value = AllX(width);
          } else {
            *out_value = MakeKnown(~normalized.value_bits + 1ull, width);
          }
          return true;
        case '~':
          {
            FourStateValue out;
            out.width = width;
            out.value_bits = ~normalized.value_bits;
            out.x_bits = normalized.x_bits;
            out.z_bits = 0;
            *out_value = ResizeValue(out, width);
          }
          return true;
        case '!':
          if (normalized.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value = MakeKnown(normalized.value_bits == 0 ? 1u : 0u, 1);
          }
          return true;
        case 'S':
          *out_value = NormalizeUnknown(value, width);
          return true;
        case 'U':
          *out_value = NormalizeUnknown(value, width);
          return true;
        case 'C': {
          if (normalized.HasXorZ()) {
            *out_value = AllX(32);
            return true;
          }
          uint64_t input = normalized.value_bits;
          uint64_t power = 1ull;
          uint64_t result = 0;
          while (power < input) {
            power <<= 1;
            ++result;
          }
          *out_value = MakeKnown(result, 32);
          return true;
        }
        case 'B': {
          if (normalized.HasXorZ()) {
            *out_value = MakeKnown(0, 1);
            return true;
          }
          *out_value = MakeKnown(normalized.value_bits != 0 ? 1u : 0u, 1);
          return true;
        }
        case '&':
          {
            uint64_t mask = MaskForWidth(width);
            uint64_t unknown = normalized.x_bits & mask;
            uint64_t known0 = (~normalized.value_bits) & ~unknown & mask;
            uint64_t known1 = normalized.value_bits & ~unknown & mask;
            if (known0 != 0) {
              *out_value = MakeKnown(0, 1);
              return true;
            }
            if (known1 == mask) {
              *out_value = MakeKnown(1, 1);
              return true;
            }
            *out_value = AllX(1);
          }
          return true;
        case '|':
          {
            uint64_t mask = MaskForWidth(width);
            uint64_t unknown = normalized.x_bits & mask;
            uint64_t known0 = (~normalized.value_bits) & ~unknown & mask;
            uint64_t known1 = normalized.value_bits & ~unknown & mask;
            if (known1 != 0) {
              *out_value = MakeKnown(1, 1);
              return true;
            }
            if (known0 == mask) {
              *out_value = MakeKnown(0, 1);
              return true;
            }
            *out_value = AllX(1);
          }
          return true;
        case '^':
          {
            uint64_t mask = MaskForWidth(width);
            if ((normalized.x_bits & mask) != 0) {
              *out_value = AllX(1);
              return true;
            }
            uint64_t bits = normalized.value_bits & mask;
            int parity = 0;
            while (bits != 0) {
              parity ^= static_cast<int>(bits & 1ull);
              bits >>= 1;
            }
            *out_value = MakeKnown(static_cast<uint64_t>(parity), 1);
          }
          return true;
        default:
          if (error) {
            *error = "unsupported unary operator in constant expression";
          }
          return false;
      }
    }
    case ExprKind::kBinary: {
      FourStateValue lhs;
      FourStateValue rhs;
      if (!EvalConstExpr4State(*expr.lhs, params, &lhs, error) ||
          !EvalConstExpr4State(*expr.rhs, params, &rhs, error)) {
        return false;
      }
      int lhs_width = ValueWidth(lhs);
      int rhs_width = ValueWidth(rhs);
      int width = std::max(lhs_width, rhs_width);
      auto clamp_width = [](int value) { return std::min(value, 64); };
      switch (expr.op) {
        case '+':
        case '-':
          width = clamp_width(std::max(lhs_width, rhs_width) + 1);
          break;
        case '*':
          width = clamp_width(lhs_width + rhs_width);
          break;
        case 'p':
          width = clamp_width(lhs_width);
          break;
        case '/':
        case '%':
          width = clamp_width(std::max(lhs_width, rhs_width));
          break;
        case 'l':
        case 'r':
        case 'R':
          width = clamp_width(lhs_width);
          break;
        default:
          width = clamp_width(width);
          break;
      }
      FourStateValue left = NormalizeUnknown(lhs, width);
      FourStateValue right = NormalizeUnknown(rhs, width);
      uint64_t mask = MaskForWidth(width);
      bool signed_cmp = ExprIsSigned(*expr.lhs) && ExprIsSigned(*expr.rhs);
      switch (expr.op) {
        case '+':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(width);
          } else {
            *out_value = MakeKnown(left.value_bits + right.value_bits, width);
          }
          return true;
        case '-':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(width);
          } else {
            *out_value = MakeKnown(left.value_bits - right.value_bits, width);
          }
          return true;
        case '*':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(width);
          } else {
            *out_value = MakeKnown(left.value_bits * right.value_bits, width);
          }
          return true;
        case 'p':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(width);
          } else {
            uint64_t base = left.value_bits & mask;
            uint64_t exp = right.value_bits & mask;
            if (signed_cmp) {
              int64_t signed_exp = SignedValue(exp, width);
              if (signed_exp < 0) {
                *out_value = MakeKnown(0, width);
                return true;
              }
              exp = static_cast<uint64_t>(signed_exp);
            }
            uint64_t result = 1ull;
            while (exp != 0) {
              if (exp & 1ull) {
                result = (result * base) & mask;
              }
              base = (base * base) & mask;
              exp >>= 1ull;
            }
            *out_value = MakeKnown(result, width);
          }
          return true;
        case '/':
          if (left.HasXorZ() || right.HasXorZ() ||
              right.value_bits == 0) {
            *out_value = AllX(width);
          } else {
            *out_value = MakeKnown(left.value_bits / right.value_bits, width);
          }
          return true;
        case '%':
          if (left.HasXorZ() || right.HasXorZ() ||
              right.value_bits == 0) {
            *out_value = AllX(width);
          } else {
            *out_value = MakeKnown(left.value_bits % right.value_bits, width);
          }
          return true;
        case '&':
          {
            uint64_t a_unknown = left.x_bits;
            uint64_t b_unknown = right.x_bits;
            uint64_t a0 = (~left.value_bits) & ~a_unknown & mask;
            uint64_t b0 = (~right.value_bits) & ~b_unknown & mask;
            uint64_t a1 = left.value_bits & ~a_unknown & mask;
            uint64_t b1 = right.value_bits & ~b_unknown & mask;
            uint64_t known0 = a0 | b0;
            uint64_t known1 = a1 & b1;
            uint64_t unknown = mask & ~(known0 | known1);
            FourStateValue out;
            out.width = width;
            out.value_bits = known1;
            out.x_bits = unknown;
            out.z_bits = 0;
            *out_value = out;
          }
          return true;
        case '|':
          {
            uint64_t a_unknown = left.x_bits;
            uint64_t b_unknown = right.x_bits;
            uint64_t a0 = (~left.value_bits) & ~a_unknown & mask;
            uint64_t b0 = (~right.value_bits) & ~b_unknown & mask;
            uint64_t a1 = left.value_bits & ~a_unknown & mask;
            uint64_t b1 = right.value_bits & ~b_unknown & mask;
            uint64_t known1 = a1 | b1;
            uint64_t known0 = a0 & b0;
            uint64_t unknown = mask & ~(known0 | known1);
            FourStateValue out;
            out.width = width;
            out.value_bits = known1;
            out.x_bits = unknown;
            out.z_bits = 0;
            *out_value = out;
          }
          return true;
        case '^':
          {
            uint64_t unknown = (left.x_bits | right.x_bits) & mask;
            FourStateValue out;
            out.width = width;
            out.value_bits = (left.value_bits ^ right.value_bits) & ~unknown &
                             mask;
            out.x_bits = unknown;
            out.z_bits = 0;
            *out_value = out;
          }
          return true;
        case 'A':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value =
                MakeKnown((left.value_bits != 0 && right.value_bits != 0) ? 1
                                                                          : 0,
                          1);
          }
          return true;
        case 'O':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value =
                MakeKnown((left.value_bits != 0 || right.value_bits != 0) ? 1
                                                                          : 0,
                          1);
          }
          return true;
        case 'E':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value =
                MakeKnown(left.value_bits == right.value_bits ? 1 : 0, 1);
          }
          return true;
        case 'N':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value =
                MakeKnown(left.value_bits != right.value_bits ? 1 : 0, 1);
          }
          return true;
        case 'C':
        case 'c': {
          FourStateValue left_raw = ResizeValue(lhs, width);
          FourStateValue right_raw = ResizeValue(rhs, width);
          uint64_t val_diff =
              (left_raw.value_bits ^ right_raw.value_bits) & mask;
          uint64_t x_diff = (left_raw.x_bits ^ right_raw.x_bits) & mask;
          uint64_t z_diff = (left_raw.z_bits ^ right_raw.z_bits) & mask;
          bool equal = (val_diff | x_diff | z_diff) == 0;
          *out_value = MakeKnown((expr.op == 'c') ? !equal : equal, 1);
          return true;
        }
        case 'W':
        case 'w': {
          FourStateValue left_raw = ResizeValue(lhs, width);
          FourStateValue right_raw = ResizeValue(rhs, width);
          uint64_t ignore = (right_raw.x_bits | right_raw.z_bits) & mask;
          uint64_t cared = (~ignore) & mask;
          if ((left_raw.x_bits | left_raw.z_bits) & cared) {
            *out_value = MakeKnown(expr.op == 'w' ? 1 : 0, 1);
            return true;
          }
          bool equal =
              (((left_raw.value_bits ^ right_raw.value_bits) & cared) == 0);
          *out_value = MakeKnown((expr.op == 'w') ? !equal : equal, 1);
          return true;
        }
        case '<':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            bool result = signed_cmp
                              ? (SignedValue(left.value_bits, width) <
                                 SignedValue(right.value_bits, width))
                              : (left.value_bits < right.value_bits);
            *out_value = MakeKnown(result ? 1 : 0, 1);
          }
          return true;
        case '>':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            bool result = signed_cmp
                              ? (SignedValue(left.value_bits, width) >
                                 SignedValue(right.value_bits, width))
                              : (left.value_bits > right.value_bits);
            *out_value = MakeKnown(result ? 1 : 0, 1);
          }
          return true;
        case 'L':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            bool result = signed_cmp
                              ? (SignedValue(left.value_bits, width) <=
                                 SignedValue(right.value_bits, width))
                              : (left.value_bits <= right.value_bits);
            *out_value = MakeKnown(result ? 1 : 0, 1);
          }
          return true;
        case 'G':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            bool result = signed_cmp
                              ? (SignedValue(left.value_bits, width) >=
                                 SignedValue(right.value_bits, width))
                              : (left.value_bits >= right.value_bits);
            *out_value = MakeKnown(result ? 1 : 0, 1);
          }
          return true;
        case 'l':
          if (right.HasXorZ()) {
            *out_value = AllX(width);
            return true;
          }
          {
            uint64_t shift = right.value_bits;
            if (shift >= static_cast<uint64_t>(width)) {
              *out_value = MakeKnown(0, width);
              return true;
            }
            FourStateValue out;
            out.width = width;
            out.value_bits = (left.value_bits << shift) & mask;
            out.x_bits = (left.x_bits << shift) & mask;
            out.z_bits = 0;
            *out_value = out;
          }
          return true;
        case 'r':
          if (right.HasXorZ()) {
            *out_value = AllX(width);
            return true;
          }
          {
            uint64_t shift = right.value_bits;
            if (shift >= static_cast<uint64_t>(width)) {
              *out_value = MakeKnown(0, width);
              return true;
            }
            FourStateValue out;
            out.width = width;
            out.value_bits = (left.value_bits >> shift) & mask;
            out.x_bits = (left.x_bits >> shift) & mask;
            out.z_bits = 0;
            *out_value = out;
          }
          return true;
        case 'R':
          if (right.HasXorZ()) {
            *out_value = AllX(width);
            return true;
          }
          {
            uint64_t shift = right.value_bits;
            if (shift >= static_cast<uint64_t>(width)) {
              *out_value = MakeKnown(0, width);
              return true;
            }
            FourStateValue out;
            out.width = width;
            out.value_bits = (left.value_bits >> shift) & mask;
            out.x_bits = (left.x_bits >> shift) & mask;
            out.z_bits = 0;
            *out_value = out;
          }
          return true;
        default:
          if (error) {
            *error = "unsupported operator in constant expression";
          }
          return false;
      }
    }
    case ExprKind::kTernary: {
      FourStateValue cond;
      if (!EvalConstExpr4State(*expr.condition, params, &cond, error)) {
        return false;
      }
      if (cond.HasXorZ()) {
        FourStateValue then_value;
        FourStateValue else_value;
        if (!EvalConstExpr4State(*expr.then_expr, params, &then_value,
                                 error) ||
            !EvalConstExpr4State(*expr.else_expr, params, &else_value,
                                 error)) {
          return false;
        }
        *out_value = MergeUnknown(then_value, else_value);
        return true;
      }
      if (cond.value_bits != 0) {
        return EvalConstExpr4State(*expr.then_expr, params, out_value, error);
      }
      return EvalConstExpr4State(*expr.else_expr, params, out_value, error);
    }
    case ExprKind::kSelect:
      if (error) {
        *error = "bit/part select not allowed in constant expression";
      }
      return false;
    case ExprKind::kIndex:
      if (error) {
        *error = "indexing not allowed in constant expression";
      }
      return false;
    case ExprKind::kConcat:
      if (error) {
        *error = "concatenation not allowed in constant expression";
      }
      return false;
    case ExprKind::kCall:
      if (error) {
        *error = "function call not allowed in constant expression";
      }
      return false;
  }
  if (error) {
    *error = "unsupported constant expression";
  }
  return false;
}

bool EvalConstExpr(const Expr& expr,
                   const std::unordered_map<std::string, int64_t>& params,
                   int64_t* out_value, std::string* error) {
  FourStateValue value;
  if (!EvalConstExpr4State(expr, params, &value, error)) {
    return false;
  }
  if (value.HasXorZ()) {
    if (error) {
      *error = "x/z not allowed in constant expression";
    }
    return false;
  }
  if (out_value) {
    *out_value = static_cast<int64_t>(value.value_bits);
  }
  return true;
}

}  // namespace gpga
