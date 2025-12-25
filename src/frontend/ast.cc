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
  out->number = expr.number;
  out->value_bits = expr.value_bits;
  out->x_bits = expr.x_bits;
  out->z_bits = expr.z_bits;
  out->number_width = expr.number_width;
  out->has_width = expr.has_width;
  out->has_base = expr.has_base;
  out->base_char = expr.base_char;
  out->op = expr.op;
  out->unary_op = expr.unary_op;
  out->msb = expr.msb;
  out->lsb = expr.lsb;
  out->has_range = expr.has_range;
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
                      : std::max({MinimalWidth(expr.value_bits),
                                  MinimalWidth(expr.x_bits),
                                  MinimalWidth(expr.z_bits)});
      FourStateValue out;
      out.width = width;
      out.value_bits = expr.value_bits;
      out.x_bits = expr.x_bits;
      out.z_bits = expr.z_bits;
      *out_value = ResizeValue(out, width);
      return true;
    }
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
      if (normalized.HasXorZ()) {
        *out_value = AllX(width);
        return true;
      }
      switch (expr.unary_op) {
        case '+':
          *out_value = NormalizeUnknown(value, width);
          return true;
        case '-':
          *out_value = MakeKnown(~normalized.value_bits + 1ull, width);
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
      int width = std::max(ValueWidth(lhs), ValueWidth(rhs));
      FourStateValue left = NormalizeUnknown(lhs, width);
      FourStateValue right = NormalizeUnknown(rhs, width);
      uint64_t mask = MaskForWidth(width);
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
        case '/':
          if (left.HasXorZ() || right.HasXorZ() ||
              right.value_bits == 0) {
            *out_value = AllX(width);
          } else {
            *out_value = MakeKnown(left.value_bits / right.value_bits, width);
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
        case '<':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value =
                MakeKnown(left.value_bits < right.value_bits ? 1 : 0, 1);
          }
          return true;
        case '>':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value =
                MakeKnown(left.value_bits > right.value_bits ? 1 : 0, 1);
          }
          return true;
        case 'L':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value =
                MakeKnown(left.value_bits <= right.value_bits ? 1 : 0, 1);
          }
          return true;
        case 'G':
          if (left.HasXorZ() || right.HasXorZ()) {
            *out_value = AllX(1);
          } else {
            *out_value =
                MakeKnown(left.value_bits >= right.value_bits ? 1 : 0, 1);
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
