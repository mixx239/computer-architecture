#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <algorithm>


struct Arguments {
    bool is_half_precision;
    int format_A;
    int format_B;
    int rounding;
    int arg_case;
    char operation;
    bool is_fma;
    uint32_t num_1;
    uint32_t num_2;
    uint32_t num_3;
    uint16_t num_1_h;
    uint16_t num_2_h;
    uint16_t num_3_h;
    bool error;
};

Arguments Parser(int argc, char** argv) {
    Arguments result;
    result.error = false;
    result.is_fma = false;
    result.is_half_precision = false;

    if (argc != 4 && argc != 6 && argc != 7) {
        result.error = true;
        return result;
    }
    if (argv[1][0] == 'h') {
        result.is_half_precision = true;
    }
    result.rounding = std::stoi(argv[2]);
    if (argc == 4) {
        result.arg_case = 1;
        if (result.is_half_precision) {
            result.num_1_h = std::stoul(argv[3], nullptr, 16);
        } else {
            result.num_1 = std::stoul(argv[3], nullptr, 16);
        }
    } else if (argc == 6) {
        result.arg_case = 2;
        std::string operation = argv[3];
        result.operation = operation.back();
        if (result.is_half_precision) {
            result.num_1_h = std::stoul(argv[4], nullptr, 16);
            result.num_2_h = std::stoul(argv[5], nullptr, 16);
        } else {
            result.num_1 = std::stoul(argv[4], nullptr, 16);
            result.num_2 = std::stoul(argv[5], nullptr, 16);
        }
    } else {
        result.arg_case = 3;
        if (std::string(argv[3]) == "fma") {
            result.is_fma = true;
        }
        if (result.is_half_precision) {
            result.num_1_h = std::stoul(argv[4], nullptr, 16);
            result.num_2_h = std::stoul(argv[5], nullptr, 16);
            result.num_3_h = std::stoul(argv[6], nullptr, 16);
        } else {
            result.num_1 = std::stoul(argv[4], nullptr, 16);
            result.num_2 = std::stoul(argv[5], nullptr, 16);
            result.num_3 = std::stoul(argv[6], nullptr, 16);
        }
    }
    return result;
}

bool IsNan(uint32_t num, int mant_len, int exp_len) {
    uint32_t exp_mask = ((1 << exp_len) - 1) << mant_len;
    uint32_t mant_mask = (1 << mant_len) - 1;
    uint32_t exp = (num & exp_mask) >> mant_len; 
    uint32_t mant = num & mant_mask;
    return ((exp == ((1 << exp_len) - 1)) && (mant != 0)); 
    
}
bool IsInf(uint32_t num, int mant_len, int exp_len) {
    uint32_t exp_mask = ((1 << exp_len) - 1) << mant_len;
    uint32_t mant_mask = (1 << mant_len) - 1;
    uint32_t exp = (num & exp_mask) >> mant_len; 
    uint32_t mant = num & mant_mask;
    return ((exp == ((1 << exp_len) - 1)) && (mant == 0));
}


uint32_t Plus(uint32_t num_1, uint32_t num_2, int mant_len, int exp_len, int rounding_type) {
    if ((num_1 == (1U << (mant_len + exp_len))) && (num_1 == num_2)) {
        return (1U << (mant_len + exp_len));
    }
    if ((num_1 == 0) && (num_2 == 0)) {
        return 0;
    }
    if (((num_1 & ((1U << (mant_len + exp_len)) - 1)) == (num_2 & ((1U << (mant_len + exp_len)) - 1))) && ((num_1 & (1U << (mant_len + exp_len))) != (num_2 & (1U << (mant_len + exp_len))))) {
        if (rounding_type == 3) {
            return (1U << (mant_len + exp_len));
        }
        return 0;
    }
    uint32_t exp_mask = ((1U << exp_len) - 1) << mant_len;
    uint32_t mant_mask = (1U << mant_len) - 1;
    uint32_t sign_mask = 1U << (mant_len + exp_len);
    uint32_t exp_1 = (num_1 & exp_mask) >> mant_len; 
    uint32_t exp_2 = (num_2 & exp_mask) >> mant_len; 
    uint32_t mant_1 = num_1 & mant_mask;
    uint32_t mant_2 = num_2 & mant_mask;
    if (exp_1 != 0) {
        mant_1 |= (1 << mant_len);
    }
    if (exp_2 != 0) {
        mant_2 |= (1 << mant_len);
    }
    uint32_t sum_mant = 0;
    int32_t sum_exp = 0;
    uint32_t exp_dif = 0;
    uint32_t sum_sign = 0;
    uint32_t sum = 0;
    std::string rounding_bits = "";
    bool special_rounding_bit = false;
    if (exp_2 == 0) {
        exp_2++;
    }
    if (exp_1 == 0) {
        exp_1++;
    }

    if (exp_1 > exp_2) {
        exp_dif = exp_1 - exp_2;
        sum_exp = exp_1;
        for (int i = 0; i < exp_dif; ++i) {
            rounding_bits = std::to_string(mant_2 % 2) + rounding_bits;
            mant_2 = mant_2 >> 1;
        }
    } else {
        exp_dif = exp_2 - exp_1;
        sum_exp = exp_2;
        for (int i = 0; i < exp_dif; ++i) {
            rounding_bits = std::to_string(mant_1 % 2) + rounding_bits;
            mant_1 = mant_1 >> 1;
        }
    }

    if ((num_1 & sign_mask) == (num_2 & sign_mask)) {
        sum_sign = num_1 & sign_mask;
        sum_mant = mant_1 + mant_2;
        if (sum_mant & (1 << (mant_len + 1))) {
            rounding_bits = std::to_string(sum_mant % 2) + rounding_bits;
            sum_mant = sum_mant >> 1;
            sum_exp++;
        }
        
        if (rounding_type == 1) {
            if ((rounding_bits.length() > 0) && (rounding_bits[0] == '1')) {
                if (std::find(rounding_bits.begin() + 1, rounding_bits.end(), '1') != rounding_bits.end()) {
                    sum_mant++;
                } else {
                    if ((sum_mant % 2) == 1) {
                        sum_mant++;
                    }
                }
            }
        } else if (rounding_type == 2) {
            if (sum_sign == 0) {
                if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                    sum_mant++;
                }
            }
        } else if (rounding_type == 3) {
            if (sum_sign != 0) {
                if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                    sum_mant++;
                }
            }
        }
        if(sum_mant & (1 << (mant_len + 1))) {
            sum_mant = sum_mant >> 1;
            sum_exp++;
        } else if ((!(sum_mant & (1 << mant_len))) && (sum_exp > 0)) {
            sum_exp--;
        }
    } else {
        if (mant_1 > mant_2) {
            sum_exp = exp_1;
            sum_sign = num_1 & sign_mask;
            sum_mant = mant_1 - mant_2;
        } else if (mant_1 < mant_2) {
            sum_exp = exp_2;
            sum_sign = num_2 & sign_mask;
            sum_mant = mant_2 - mant_1;
        } else {
            return 0;
        }
        while (((sum_mant & (1 << mant_len)) == 0) && (sum_exp > 0)) {
            sum_mant = sum_mant << 1;
            if ((rounding_bits.length() > 0)) {
                sum_mant -= (rounding_bits[0] - '0');
                rounding_bits.erase(0, 1);
            }
            sum_exp--;
            if (sum_exp == 0) {
                rounding_bits = std::to_string(sum_mant % 2) + rounding_bits;
                sum_mant = sum_mant >> 1;
            }
        }
        if (rounding_type == 0) {
            if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                sum_mant--;
            }
        } else if (rounding_type == 1) {
            if ((rounding_bits.length() > 0) && (rounding_bits[0] == '1')) {
                if ((std::find(rounding_bits.begin() + 1, rounding_bits.end(), '1') != rounding_bits.end())) {
                    sum_mant--;
                } else {
                    if ((sum_mant % 2) == 1) {
                        sum_mant--;
                    }
                }
            }
        } else if (rounding_type == 2) {
            if (sum_sign != 0) {
                if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                    sum_mant--;
                    if (sum_mant == mant_mask) {
                        special_rounding_bit = true;
                    }
                }
            }
        } else if (rounding_type == 3) {
            if (sum_sign == 0) {
                if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                    sum_mant--;
                    if (sum_mant == mant_mask) {
                        special_rounding_bit = true;
                    }
                }
            }
        }
        if(sum_mant & (1 << (mant_len + 1))) {
            sum_mant = sum_mant >> 1;
            sum_exp++;
        } else if ((!(sum_mant & (1 << mant_len))) && (sum_exp > 0)) {
            sum_mant = sum_mant << 1;
            if (special_rounding_bit) {
                sum_mant++;
            }
            sum_exp--;
        }
    }
    if (sum_exp >= ((1 << exp_len) - 1)) {
        if ((rounding_type == 0) || ((rounding_type == 2) && (sum_sign != 0)) || ((rounding_type == 3) && (sum_sign == 0))) {
            return (sum_sign | ((((1 << (exp_len - 1)) - 1) << (mant_len + 1)) | mant_mask));
        }
        return (sum_sign | exp_mask);
    }
    sum = sum_sign | (sum_exp << mant_len) | (sum_mant & mant_mask);
    return sum;
}


uint32_t Mult(uint32_t num_1, uint32_t num_2, int mant_len, int exp_len, int rounding_type) {
    uint64_t exp_mask = ((1ULL << exp_len) - 1) << mant_len;
    uint64_t mant_mask = (1ULL << mant_len) - 1;
    uint64_t sign_mask = 1ULL << (mant_len + exp_len);
    int64_t exp_1 = (num_1 & exp_mask) >> mant_len; 
    int64_t exp_2 = (num_2 & exp_mask) >> mant_len; 
    uint64_t mant_1 = num_1 & mant_mask;
    uint64_t mant_2 = num_2 & mant_mask;
    if (exp_1 != 0) {
        mant_1 |= (1 << mant_len);
    }
    if (exp_2 != 0) {
        mant_2 |= (1 << mant_len);
    }
    uint64_t mult_mant = 0;
    int64_t mult_exp = 0;
    int64_t mult_sign = 0;
    uint64_t mult = 0;
    std::string rounding_bits = "";
    if (exp_2 == 0) {
        exp_2++;
    }
    if (exp_1 == 0) {
        exp_1++;
    }

    if ((num_1 & sign_mask) != (num_2 & sign_mask)) {
        mult_sign = (1ULL << (exp_len + mant_len));
    } 
    if (IsInf(num_1, mant_len, exp_len) || IsInf(num_2, mant_len, exp_len)) {
        return (mult_sign | exp_mask);
    }

    mult_mant = mant_1 * mant_2;
    mult_exp = exp_1 + exp_2 - ((1ULL << (exp_len - 1)) - 1);
    
    for (int i = 0; i < mant_len; ++i) {
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
    }
    if (mult_exp == 0) {
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
        mult_exp++;
    } 
    if (mult_exp < 0) {
        while (mult_exp < 0) {
            rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
            mult_mant = mult_mant >> 1;
            mult_exp++;
        }
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
    }
   
    while (mult_mant > (mult_mant & ((1ULL << (mant_len + 1)) - 1))) {
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
        mult_exp++;
    }

    while ((((mult_mant & (1ULL << mant_len)) == 0)) && (mult_exp > 0)) {
        mult_mant = mult_mant << 1;
        if ((rounding_bits.length() > 0)) {
            mult_mant += (rounding_bits[0] - '0');
            rounding_bits.erase(0, 1);
        }
        mult_exp--;
        if (mult_exp == 0) {
            rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
            mult_mant = mult_mant >> 1;
        }
    }

    if (rounding_type == 1) {
        if ((rounding_bits.length() > 0) && (rounding_bits[0] == '1')) {
            if ((std::find(rounding_bits.begin() + 1, rounding_bits.end(), '1') != rounding_bits.end())) {
                mult_mant++;
            } else {
                if ((mult_mant % 2) == 1) {
                    mult_mant++;
                }
            }
        }
    } else if (rounding_type == 2) {
        if (mult_sign == 0) {
            if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                mult_mant++;
            }
        }
    } else if (rounding_type == 3) {
        if (mult_sign != 0) {
            if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                mult_mant++;
            }
        }
    }
    if (mult_mant > (mult_mant & ((1ULL << (mant_len + 1)) - 1))) {
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
        mult_exp++;
    }

    if (mult_exp >= ((1ULL << exp_len) - 1)) {
        if ((rounding_type == 0) || ((rounding_type == 2) && (mult_sign != 0)) || ((rounding_type == 3) && (mult_sign == 0))) {
            return (mult_sign | ((((1 << (exp_len - 1)) - 1) << (mant_len + 1)) | mant_mask));
        }
        return (mult_sign | exp_mask);
    }
    if ((mult_exp < 0) || (((mult_exp == 0) && (mult_mant == 0))))  {
        return mult_sign;
    }
    mult = mult_sign | (mult_exp << mant_len) | (mult_mant & mant_mask);
    return mult;
}


uint32_t Division(uint32_t num_1, uint32_t num_2, int mant_len, int exp_len, int rounding_type) {
    uint64_t exp_mask = ((1ULL << exp_len) - 1) << mant_len;
    uint64_t mant_mask = (1ULL << mant_len) - 1;
    uint64_t sign_mask = 1ULL << (mant_len + exp_len);
    uint64_t exp_1 = (num_1 & exp_mask) >> mant_len; 
    uint64_t exp_2 = (num_2 & exp_mask) >> mant_len; 
    uint64_t mant_1 = num_1 & mant_mask;
    uint64_t mant_2 = num_2 & mant_mask;
    if (exp_1 != 0) {
        mant_1 |= (1 << mant_len);
    }
    if (exp_2 != 0) {
        mant_2 |= (1 << mant_len);
    }
    uint64_t div_mant = 0;
    int64_t div_exp = 0;
    uint64_t div_sign = 0;
    uint64_t div = 0;
    std::string rounding_bits = "";
    uint32_t remainder = 0;
    if (exp_2 == 0) {
        exp_2++;
    }
    if (exp_1 == 0) {
        exp_1++;
    }
    if ((num_1 & sign_mask) != (num_2 & sign_mask)) {
        div_sign = (1ULL << (exp_len + mant_len));
    } 
    if (((num_2 & ((1ULL << (mant_len + exp_len)) - 1)) == 0) || (IsInf(num_1, mant_len, exp_len))) {
        return (div_sign | exp_mask);
    }
    if (IsInf(num_2, mant_len, exp_len)) {
        return div_sign;
    }
    mant_1 = mant_1 << mant_len;
    div_mant = mant_1 / mant_2;
    remainder = mant_1 % mant_2;
    int bit;
    for (int i = 0; i < 300; ++i) {
        remainder *= 2;
        bit = remainder / mant_2;
        rounding_bits += std::to_string(bit);
        remainder %= mant_2;
    }
    if (remainder > 0) {
        rounding_bits += '1';
    } else {
        rounding_bits += '0';
    }
    div_exp = exp_1 - exp_2 + ((1ULL << (exp_len - 1)) - 1);

    if (div_exp == 0) {
        rounding_bits = std::to_string(div_mant % 2) + rounding_bits;
        div_mant = div_mant >> 1;
        div_exp++;
    }
    while (div_mant > (div_mant & ((1ULL << (mant_len + 1)) - 1))) {
        rounding_bits = std::to_string(div_mant % 2) + rounding_bits;
        div_mant = div_mant >> 1;
        div_exp++;
    }
    if (div_exp < 0) {
        while (div_exp < 0) {
            rounding_bits = std::to_string(div_mant % 2) + rounding_bits;
            div_mant = div_mant >> 1;
            div_exp++;
        }
        rounding_bits = std::to_string(div_mant % 2) + rounding_bits;
        div_mant = div_mant >> 1;
    }
    
    while ((div_mant < ((1ULL << mant_len))) && (div_exp > 0)) {
        div_mant = div_mant << 1;
        div_mant += (rounding_bits[0] - '0');
        rounding_bits.erase(0, 1);
        div_exp--;
        if (div_exp == 0) {
            rounding_bits = std::to_string(div_mant % 2) + rounding_bits;
            div_mant = div_mant >> 1;
        }
    }

    if (rounding_type == 1) {
        if ((rounding_bits.length() > 0) && (rounding_bits[0] == '1')) {
            if ((std::find(rounding_bits.begin() + 1, rounding_bits.end(), '1') != rounding_bits.end())) {
                div_mant++;
            } else {
                if ((div_mant % 2) == 1) {
                    div_mant++;
                }
            }
        }
    } else if (rounding_type == 2) {
        if (div_sign == 0) {
            if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                div_mant++;
            }
        }
    } else if (rounding_type == 3) {
        if (div_sign != 0) {
            if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') != rounding_bits.end()) {
                div_mant++;
            }
        }
    }
    if (div_mant > (div_mant & ((1ULL << (mant_len + 1)) - 1))) {
        rounding_bits = std::to_string(div_mant % 2) + rounding_bits;
        div_mant = div_mant >> 1;
        div_exp++;
    }

    if (div_exp >= ((1ULL << exp_len) - 1)) {
        if ((rounding_type == 0) || ((rounding_type == 2) && (div_sign != 0)) || ((rounding_type == 3) && (div_sign == 0))) {
            return (div_sign | ((((1 << (exp_len - 1)) - 1) << (mant_len + 1)) | mant_mask));
        }
        return (div_sign | exp_mask);
    }
    if ((div_exp < 0) || (((div_exp == 0) && (div_mant == 0))))  {
        return div_sign;
    }
    div = div_sign | (div_exp << mant_len) | (div_mant & mant_mask);
    return div;
}


std::string StrMinus(std::string a, std::string b) {
    std::string res = "";
    int borrow = 0;
    int bit_a = 0;
    int bit_b = 0;
    for(int i = a.length() - 1; i >= 0; --i) {
        bit_a = a[i] - '0';
        bit_b = (b[i] - '0') + borrow;
        if (bit_a < bit_b) {
            bit_a += 2;
            borrow = 1;
        } else {
            borrow = 0;
        }
        res = std::to_string(bit_a - bit_b) + res;
    }
    return res;
}


uint32_t Fma(uint32_t num_1, uint32_t num_2, uint32_t num_3, int mant_len, int exp_len, int rounding_type) {
    uint64_t exp_mask = ((1ULL << exp_len) - 1) << mant_len;
    uint64_t mant_mask = (1ULL << mant_len) - 1;
    uint64_t sign_mask = 1ULL << (mant_len + exp_len);
    uint64_t exp_1 = (num_1 & exp_mask) >> mant_len; 
    uint64_t exp_2 = (num_2 & exp_mask) >> mant_len; 
    uint64_t exp_3 = (num_3 & exp_mask) >> mant_len; 
    uint64_t mant_1 = num_1 & mant_mask;
    uint64_t mant_2 = num_2 & mant_mask;
    uint64_t mant_3 = num_3 & mant_mask;
    if (exp_1 != 0) {
        mant_1 |= (1 << mant_len);
    }
    if (exp_2 != 0) {
        mant_2 |= (1 << mant_len);
    }
    if (exp_3 != 0) {
        mant_3 |= (1 << mant_len);
    }
    uint64_t mult_mant = 0;
    int64_t mult_exp = 0;
    int64_t exp_dif = 0;
    uint64_t mult_sign = 0;
    uint64_t sum_mant = 0;
    int64_t sum_exp = 0;
    uint64_t sum_sign = 0;
    std::string rounding_bits = "";
    std::string rounding_bits_sum = "";
    if (exp_1 == 0) {
        exp_1++;
    }
    if (exp_2 == 0) {
        exp_2++;
    }
    if (exp_3 == 0) {
        exp_3++;
    }

    if ((num_1 & sign_mask) != (num_2 & sign_mask)) {
        mult_sign = (1ULL << (exp_len + mant_len));
    } 
    if (IsInf(num_1, mant_len, exp_len) || IsInf(num_2, mant_len, exp_len)) {
        return (mult_sign | exp_mask);
    }

    mult_mant = mant_1 * mant_2;
    mult_exp = exp_1 + exp_2 - ((1ULL << (exp_len - 1)) - 1);
    
    for (int i = 0; i < mant_len; ++i) {
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
    }
    if (mult_exp == 0) {
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
        mult_exp++;
    } 
    if (mult_exp < 0) {
        while (mult_exp < 0) {
            rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
            mult_mant = mult_mant >> 1;
            mult_exp++;
        }
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
    }

    while (mult_mant > (mult_mant & ((1ULL << (mant_len + 1)) - 1))) {
        rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
        mult_mant = mult_mant >> 1;
        mult_exp++;
    }

    while ((((mult_mant & ((1ULL << mant_len))) == 0)) && (mult_exp > 0)) {
        mult_mant = mult_mant << 1;
        if ((rounding_bits.length() > 0)) {
            mult_mant += (rounding_bits[0] - '0');
            rounding_bits.erase(0, 1);
        }
        mult_exp--;
        if (mult_exp == 0) {
            rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
            mult_mant = mult_mant >> 1;
        }
    }
    uint32_t mult = mult_sign | (mult_exp << mant_len) | (mult_mant & mant_mask);
    if ((mult == (1U << (mant_len + exp_len))) && (mult == num_3) && (std::find(rounding_bits.begin(), rounding_bits.end(), '1') == rounding_bits.end())) {
        return (1U << (mant_len + exp_len));
    }
    if ((mult == 0) && (num_3 == 0) && (std::find(rounding_bits.begin(), rounding_bits.end(), '1') == rounding_bits.end())) {
        return 0;
    }
    if (((mult & ((1U << (mant_len + exp_len)) - 1)) == (num_3 & ((1U << (mant_len + exp_len)) - 1))) && ((mult & (1U << (mant_len + exp_len))) != (num_3 & (1U << (mant_len + exp_len))))) {
        if (std::find(rounding_bits.begin(), rounding_bits.end(), '1') == rounding_bits.end()) {    
            if (rounding_type == 3) {
                return (1U << (mant_len + exp_len));
            }
            return 0;
        }
    }
    uint32_t sum = 0;
    if (mult_exp == 0) {
        mult_exp++;
    }
    if (mult_exp > exp_3) {
        exp_dif = mult_exp - exp_3;
        sum_exp = mult_exp;
        for (int i = 0; i < exp_dif; ++i) {
            rounding_bits_sum = std::to_string(mant_3 % 2) + rounding_bits_sum;
            mant_3 = mant_3 >> 1;
        }
    } else {
        exp_dif = exp_3 - mult_exp;
        sum_exp = exp_3;
        for (int i = 0; i < exp_dif; ++i) {
            rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
            mult_mant = mult_mant >> 1;
        }
    }

    if ((num_3 & sign_mask) == mult_sign) {
        int max_len = std::max(rounding_bits_sum.length(), rounding_bits.length());
        rounding_bits_sum = rounding_bits_sum + std::string(max_len - rounding_bits_sum.length(), '0');
        rounding_bits = rounding_bits + std::string(max_len - rounding_bits.length(), '0');
        std::string result_rounding_bits = "";
        int carry = 0;
        int sum_bits = 0;
        for (int i = max_len - 1; i >= 0; --i) {
            sum_bits = (rounding_bits[i] - '0') + (rounding_bits_sum[i] - '0') + carry;
            if (sum_bits == 0) {
                result_rounding_bits = "0" + result_rounding_bits;
                carry = 0;
            }
            if (sum_bits == 1) {
                result_rounding_bits = "1" + result_rounding_bits;
                carry = 0;
            }
            if (sum_bits == 2) {
                result_rounding_bits = "0" + result_rounding_bits;
                carry = 1;
            }
            if (sum_bits == 3) {
                result_rounding_bits = "1" + result_rounding_bits;
                carry = 1;
            }
        }
        if (carry) {
            sum_mant = 1;
        }
        sum_sign = mult_sign;
        sum_mant += mant_3 + mult_mant;
        if (sum_mant & (1 << (mant_len + 1))) {
            result_rounding_bits = std::to_string(sum_mant % 2) + result_rounding_bits;
            sum_mant = sum_mant >> 1;
            sum_exp++;
        }
        
        if (rounding_type == 1) {
            if ((result_rounding_bits.length() > 0) && (result_rounding_bits[0] == '1')) {
                if (std::find(result_rounding_bits.begin() + 1, result_rounding_bits.end(), '1') != result_rounding_bits.end()) {
                    sum_mant++;
                } else {
                    if ((sum_mant % 2) == 1) {
                        sum_mant++;
                    }
                }
            }
        } else if (rounding_type == 2) {
            if (sum_sign == 0) {
                if (std::find(result_rounding_bits.begin(), result_rounding_bits.end(), '1') != result_rounding_bits.end()) {
                    sum_mant++;
                }
            }
        } else if (rounding_type == 3) {
            if (sum_sign != 0) {
                if (std::find(result_rounding_bits.begin(), result_rounding_bits.end(), '1') != result_rounding_bits.end()) {
                    sum_mant++;
                }
            }
        }
        if (sum_mant & (1 << (mant_len + 1))) {
            sum_mant = sum_mant >> 1;
            sum_exp++;
        } else if ((!(sum_mant & (1 << mant_len))) && (sum_exp > 0)) {
            sum_exp--;
        }
    } else {
        for (int i = 0; i <= mant_len; ++i) {
            rounding_bits = std::to_string(mult_mant % 2) + rounding_bits;
            mult_mant = mult_mant >> 1;
            rounding_bits_sum = std::to_string(mant_3 % 2) + rounding_bits_sum;
            mant_3 = mant_3 >> 1;
        }
        int max_len = std::max(rounding_bits_sum.length(), rounding_bits.length());
        rounding_bits_sum = rounding_bits_sum + std::string(max_len - rounding_bits_sum.length(), '0');
        rounding_bits = rounding_bits + std::string(max_len - rounding_bits.length(), '0');
        std::string result_rounding_bits = "";
        if (rounding_bits > rounding_bits_sum) {
            sum_exp = mult_exp;
            sum_sign = mult_sign;
            result_rounding_bits = StrMinus(rounding_bits, rounding_bits_sum);
        } else { 
            sum_exp = exp_3;
            sum_sign = num_3 & sign_mask;
            result_rounding_bits = StrMinus(rounding_bits_sum, rounding_bits);
        }
        sum_mant = 0;
        for (int i = 0; i <= mant_len; ++i) {
            sum_mant = sum_mant << 1;
            sum_mant += (result_rounding_bits[0] - '0');
            result_rounding_bits.erase(0, 1);
        }

        while (((sum_mant & (1 << mant_len)) == 0) && (sum_exp > 0)) {
            sum_mant = sum_mant << 1;
            if ((result_rounding_bits.length() > 0)) {
                sum_mant += (result_rounding_bits[0] - '0');
                result_rounding_bits.erase(0, 1);
            }
            sum_exp--;
            if (sum_exp == 0) {
                result_rounding_bits = std::to_string(sum_mant % 2) + result_rounding_bits;
                sum_mant = sum_mant >> 1;
            }
        }
        if (rounding_type == 1) {
            if ((result_rounding_bits.length() > 0) && (result_rounding_bits[0] == '1')) {
                if ((std::find(result_rounding_bits.begin() + 1, result_rounding_bits.end(), '1') != result_rounding_bits.end())) {
                    sum_mant++;
                } else {
                    if ((sum_mant % 2) == 1) {
                        sum_mant++;
                    }
                }
            }
        } else if (rounding_type == 2) {
            if (sum_sign == 0) {
                if (std::find(result_rounding_bits.begin(), result_rounding_bits.end(), '1') != result_rounding_bits.end()) {
                    sum_mant++;
                }
            }
        } else if (rounding_type == 3) {
            if (sum_sign != 0) {
                if (std::find(result_rounding_bits.begin(), result_rounding_bits.end(), '1') != result_rounding_bits.end()) {
                    sum_mant++;
                }
            }
        }
        if(sum_mant & (1 << (mant_len + 1))) {
            sum_mant = sum_mant >> 1;
            sum_exp++;
        } else if ((!(sum_mant & (1 << mant_len))) && (sum_exp > 0)) {
            sum_exp--;
        }
    }
    if (sum_exp >= ((1 << exp_len) - 1)) {
        if ((rounding_type == 0) || ((rounding_type == 2) && (sum_sign != 0)) || ((rounding_type == 3) && (sum_sign == 0))) {
            return (sum_sign | ((((1 << (exp_len - 1)) - 1) << (mant_len + 1)) | mant_mask));
        }
        return (sum_sign | exp_mask);
    }
    sum = sum_sign | (sum_exp << mant_len) | (sum_mant & mant_mask);
    return sum;
}


void Print(uint32_t num, int mant_len, int exp_len) {
    if ((num << (32 - mant_len - exp_len)) == 0) {
        if (num > 0) {
            std::cout << "-";
        }
        if (mant_len == 23) {
          std::cout << "0x0.000000p+0 ";
        } else {
            std::cout << "0x0.000p+0 ";
        }
    } else {
        if (IsNan(num, mant_len, exp_len)) {
            std::cout << "nan ";
            num = num | (1 << (mant_len - 1));
        } else if (IsInf(num, mant_len, exp_len)) {
            if (num & (1 << (mant_len + exp_len)) ) {
                std::cout << "-inf ";
            } else {
                std::cout << "inf ";
            }
        } else {
            if (num & (1 << (mant_len + exp_len)) ) {
                std::cout << "-";
            }
            uint32_t mant_mask = (1 << mant_len) - 1;
            uint32_t exp_mask = ((1 << exp_len) - 1) << mant_len;
            uint32_t mant = num & mant_mask;
            int32_t exp = (num & exp_mask) >> mant_len;
            if (exp == 0) {
                exp++;
                while ((mant & (1 << (mant_len))) == 0) {
                    mant = mant << 1;
                    exp--;
                }
                mant = mant & mant_mask;
            }
            std::cout << "0x1.";
            if (mant_len == 23) {
                mant = mant << 1;
                exp -= 127; 
                std::cout << std::hex << std::nouppercase << std::setfill('0') << std::setw(6) << mant; 
            } else {
                mant = mant << 2;
                exp -= 15; 
                std::cout << std::hex << std::nouppercase << std::setfill('0') << std::setw(3) << mant; 
            }
            if (exp >= 0) {
                std::cout << "p+" << std::to_string(exp) << " "; 
            } else {
            std::cout << "p" << std::to_string(exp) << " "; 
            }
        }
    }
    if (mant_len == 23) {
        std::cout << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << num;
    } else {
        std::cout << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << num;
    }
}


int main(int argc, char** argv) {
    Arguments parsed_args;
    parsed_args = Parser(argc, argv);
    if (parsed_args.error)  {
        std::cerr << "Input data error";
        return 1;
    }
    uint32_t ans;
    uint16_t ans_h;

    if (parsed_args.arg_case == 1) {
        if(parsed_args.is_half_precision) {
            Print(parsed_args.num_1_h, 10, 5);
        } else {
            Print(parsed_args.num_1, 23, 8);
        }
    } else if (parsed_args.arg_case == 2) {
        if (parsed_args.is_half_precision) {
            if (IsNan(parsed_args.num_1_h, 10, 5)) {
                Print(parsed_args.num_1_h, 10, 5);
                 return 0;
            } else if (IsNan(parsed_args.num_2_h, 10, 5)) {
                Print(parsed_args.num_2_h, 10, 5);
                return 0;
            } 
        } else  {
            if (IsNan(parsed_args.num_1, 23, 8)) {
                Print(parsed_args.num_1, 23, 8);
                return 0;
            } else if (IsNan(parsed_args.num_2, 23, 8)) {
                Print(parsed_args.num_2, 23, 8);
                return 0;
            }
        }

        if (parsed_args.operation == '+') {
            if (parsed_args.is_half_precision) {
                if (IsInf(parsed_args.num_2_h, 10, 5) && IsInf(parsed_args.num_1_h, 10, 5) && ((parsed_args.num_2_h & (1 << 15)) != (parsed_args.num_1_h & (1 << 15)))) {
                    ans_h = (1 << 15) | (((1 << 5) - 1) << 10) | (1 << 9);
                } else {
                    ans_h = Plus(parsed_args.num_1_h, parsed_args.num_2_h, 10, 5, parsed_args.rounding);
                }
                Print(ans_h, 10, 5);
            } else { 
                if (IsInf(parsed_args.num_2, 23, 8) && IsInf(parsed_args.num_1, 23, 8) && ((parsed_args.num_2 & (1U << 31)) != (parsed_args.num_1 & (1U << 31)))) {
                    ans = (1U << 31) | (((1 << 8) - 1) << 23) | (1 << 22);
                } else {
                    ans = Plus(parsed_args.num_1, parsed_args.num_2, 23, 8, parsed_args.rounding);
                }
                Print(ans, 23, 8);
            }
        } else if (parsed_args.operation == '-') {
            if (parsed_args.is_half_precision) {
                if (IsInf(parsed_args.num_2_h, 10, 5) && IsInf(parsed_args.num_1_h, 10, 5) && ((parsed_args.num_2_h & (1 << 15)) == (parsed_args.num_1_h & (1 << 15)))) {
                    ans_h = (1 << 15) | (((1 << 5) - 1) << 10) | (1 << 9);
                } else {
                    parsed_args.num_2_h ^= (1 << 15);
                    ans_h = Plus(parsed_args.num_1_h, parsed_args.num_2_h, 10, 5, parsed_args.rounding);
                }
                Print(ans_h, 10, 5);
            } else { 
                if (IsInf(parsed_args.num_2, 23, 8) && IsInf(parsed_args.num_1, 23, 8) && ((parsed_args.num_2 & (1U << 31)) == (parsed_args.num_1 & (1U << 31)))) {
                    ans = (1U << 31) | (((1 << 8) - 1) << 23) | (1 << 22);
                } else {
                    parsed_args.num_2 ^= (1U << 31);
                    ans = Plus(parsed_args.num_1, parsed_args.num_2, 23, 8, parsed_args.rounding);
                }
                Print(ans, 23, 8);
            }
        } else if (parsed_args.operation == '*') {
            if (parsed_args.is_half_precision) {
                if ((((parsed_args.num_1_h & ((1 << 15) - 1)) == 0) && (IsInf(parsed_args.num_2_h, 10, 5))) || (((parsed_args.num_2_h & ((1 << 15) - 1)) == 0) && (IsInf(parsed_args.num_1_h, 10, 5)))) {
                    ans_h = (1 << 15) | (((1 << 5) - 1) << 10) | (1 << 9);
                } else {
                    ans_h = Mult(parsed_args.num_1_h, parsed_args.num_2_h, 10, 5, parsed_args.rounding);
                }
                Print(ans_h, 10, 5);
            } else { 
                if ((((parsed_args.num_1 & ((1U << 31) - 1)) == 0) && (IsInf(parsed_args.num_2, 23, 8))) || (((parsed_args.num_2 & ((1U << 31) - 1)) == 0) && (IsInf(parsed_args.num_1, 23, 8)))) {
                    ans = (1U << 31) | (((1 << 8) - 1) << 23) | (1 << 22);
                } else {
                    ans = Mult(parsed_args.num_1, parsed_args.num_2, 23, 8, parsed_args.rounding);
                }
                Print(ans, 23, 8);
            }
        } else if (parsed_args.operation == '/') {
            if (parsed_args.is_half_precision) {
                if ((((parsed_args.num_2_h & ((1 << 15) - 1)) == 0) && ((parsed_args.num_1_h & ((1 << 15) - 1)) == 0)) || (IsInf(parsed_args.num_1_h, 10, 5) && IsInf(parsed_args.num_2_h, 10, 5))) {
                    ans_h = (1 << 15) | (((1 << 5) - 1) << 10) | (1 << 9);
                } else {
                    ans_h = Division(parsed_args.num_1_h, parsed_args.num_2_h, 10, 5, parsed_args.rounding);
                }
                Print(ans_h, 10, 5);
            } else { 
                if ((((parsed_args.num_2 & ((1U << 31) - 1)) == 0) && ((parsed_args.num_1 & ((1U << 31) - 1)) == 0)) || (IsInf(parsed_args.num_1, 23, 8) && IsInf(parsed_args.num_2, 23, 8))) {
                    ans = (1U << 31) | (((1 << 8) - 1) << 23) | (1 << 22);
                } else {
                    ans = Division(parsed_args.num_1, parsed_args.num_2, 23, 8, parsed_args.rounding);
                }
                Print(ans, 23, 8);
            }
        }
    } else if (parsed_args.arg_case == 3) {
        if (parsed_args.is_half_precision) {
            if (IsNan(parsed_args.num_1_h, 10, 5)) {
                Print(parsed_args.num_1_h, 10, 5);
                return 0;
            } else if (IsNan(parsed_args.num_2_h, 10, 5)) {
                Print(parsed_args.num_2_h, 10, 5);
                return 0;
            } else if (IsNan(parsed_args.num_3_h, 10, 5)) {
                Print(parsed_args.num_3_h, 10, 5);
                return 0;
            } 
        } else {
            if (IsNan(parsed_args.num_1, 23, 8)) {
                Print(parsed_args.num_1, 23, 8);
                return 0;
            } else if (IsNan(parsed_args.num_2, 23, 8)) {
                Print(parsed_args.num_2, 23, 8);
                return 0;
            }
            else if (IsNan(parsed_args.num_3, 23, 8)) {
                Print(parsed_args.num_3, 23, 8);
                return 0;
            }
        }

        if (parsed_args.is_fma) {
            if (parsed_args.is_half_precision) {
                if ((((parsed_args.num_1_h & ((1 << 15) - 1)) == 0) && (IsInf(parsed_args.num_2_h, 10, 5))) || (((parsed_args.num_2_h & ((1 << 15) - 1)) == 0) && (IsInf(parsed_args.num_1_h, 10, 5)))) {
                    ans_h = (1 << 15) | (((1 << 5) - 1) << 10) | (1 << 9);
                } else {
                    ans_h = Fma(parsed_args.num_1_h, parsed_args.num_2_h, parsed_args.num_3_h, 10, 5, parsed_args.rounding);
                }
                Print(ans_h, 10, 5);
            } else {
                if ((((parsed_args.num_1 & ((1U << 31) - 1)) == 0) && (IsInf(parsed_args.num_2, 23, 8))) || (((parsed_args.num_2 & ((1U << 31) - 1)) == 0) && (IsInf(parsed_args.num_1, 23, 8)))) {
                    ans = (1U << 31) | (((1 << 8) - 1) << 23) | (1 << 22);
                } else {
                    ans = Fma(parsed_args.num_1, parsed_args.num_2, parsed_args.num_3, 23, 8, parsed_args.rounding);
                }
                Print(ans, 23, 8);
            }
        } else {
            if (parsed_args.is_half_precision) {
                if ((((parsed_args.num_1_h & ((1 << 15) - 1)) == 0) && (IsInf(parsed_args.num_2_h, 10, 5))) || (((parsed_args.num_2_h & ((1 << 15) - 1)) == 0) && (IsInf(parsed_args.num_1_h, 10, 5)))) {
                    ans_h = (1 << 15) | (((1 << 5) - 1) << 10) | (1 << 9);
                } else {
                    ans_h = Mult(parsed_args.num_1_h, parsed_args.num_2_h, 10, 5, parsed_args.rounding);
                }
                if (IsInf(ans_h, 10, 5) && IsInf(parsed_args.num_3_h, 10, 5) && ((ans_h & (1 << 15)) != (parsed_args.num_3_h & (1 << 15)))) {
                    ans_h = (1 << 15) | (((1 << 5) - 1) << 10) | (1 << 9);
                } else if (!IsInf(ans_h, 10, 5)) {
                    ans_h = Plus(ans_h, parsed_args.num_3_h, 10, 5, parsed_args.rounding);
                }
                Print(ans_h, 10, 5);
            } else {
                if ((((parsed_args.num_1 & ((1U << 31) - 1)) == 0) && (IsInf(parsed_args.num_2, 23, 8))) || (((parsed_args.num_2 & ((1U << 31) - 1)) == 0) && (IsInf(parsed_args.num_1, 23, 8)))) {
                    ans = (1U << 31) | (((1 << 8) - 1) << 23) | (1 << 22);
                } else {
                    ans = Mult(parsed_args.num_1, parsed_args.num_2, 23, 8, parsed_args.rounding);
                }
                if (IsInf(ans, 23, 8) && IsInf(parsed_args.num_3, 23, 8) && ((ans & (1U << 31)) != (parsed_args.num_3 & (1U << 31)))) {
                    ans = (1U << 31) | (((1 << 8) - 1) << 23) | (1 << 22);
                } else if (!IsInf(ans, 23, 8)) {
                    ans = Plus(ans, parsed_args.num_3, 23, 8, parsed_args.rounding);
                }
                Print(ans, 23, 8);
            }
        }
    }
    return 0;
}