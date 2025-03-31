function decodeFloatLE(bytes, offset = 0) {
  // Combine the four bytes into a 32-bit integer (little-endian)
  // bytes[offset+0] is least significant, bytes[offset+3] is most significant
  let bits =
    bytes[offset] |
    (bytes[offset + 1] << 8) |
    (bytes[offset + 2] << 16) |
    (bytes[offset + 3] << 24);

  // Extract sign, exponent, and mantissa using IEEE 754 single-precision
  let sign = bits >>> 31 ? -1 : 1;
  let exponent = ((bits >>> 23) & 0xff) - 127;
  let mantissa = bits & 0x7fffff;

  // The implicit leading 1 for normalized values
  mantissa |= 0x800000;

  // Value = sign * mantissa * 2^(exponent - 23)
  let floatVal = sign * mantissa * Math.pow(2, exponent - 23);
  return floatVal;
}

// Main decoder function for TTN
function decodeUplink(input) {
  // We expect at least 4 bytes for a single float
  if (input.bytes.length < 4) {
    return {
      errors: ["Not enough bytes for float"],
    };
  }

  // Decode the float from the first 4 bytes
  let rollingAvg = decodeFloatLE(input.bytes, 0);

  return {
    data: {
      rolling_avg: rollingAvg,
    },
  };
}
