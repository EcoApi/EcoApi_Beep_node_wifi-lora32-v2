function Decoder(bytes, port) {
    var decoded = {};
    
    decoded.beep_base = true;
    decoded.alarm     = null;
    
    decoded.bv = 3.3;
    decoded.bat_perc = 100;
    
    decoded.fft_present = false;
    decoded.fft_bin_amount = 0;
    decoded.fft_start_bin = 0;
    decoded.fft_stop_bin = 0;
    
    var rawTemp = ((bytes[0] & 0x80) ? (0xFFFF << 16) : 0) | (bytes[0] << 8) | bytes[1];
    //decoded.temperature = sflt162f(rawTemp) * 100;
    var temperature = rawTemp / 100;
    
    var rawHumidity = (bytes[2] << 8) | bytes[3];
    //decoded.humidity = sflt162f(rawHumidity) * 100;
    var humidity = rawHumidity / 100;
    
    var rawPressure = (bytes[4] << 8) | bytes[5];
    var pressure = rawPressure;
    
    var rawWeight = (bytes[6] << 8) | bytes[7];
    decoded.weight_present = true;
    decoded.weight_sensor_amount = 1;
    decoded.w_v = rawWeight;
    
    decoded.bme280_present = true;
    decoded.t = temperature;
    decoded.h = humidity;
    decoded.p = pressure;
    
    decoded.ds18b20_present = true;
    decoded.ds18b20_sensor_amount = 1;
    decoded.t_i = temperature;
  
    return decoded;
  }
  
  function sflt162f(rawSflt16) {
      // rawSflt16 is the 2-byte number decoded from wherever;
      // it's in range 0..0xFFFF
      // bit 15 is the sign bit
      // bits 14..11 are the exponent
      // bits 10..0 are the the mantissa. Unlike IEEE format, 
      // 	the msb is transmitted; this means that numbers
      //	might not be normalized, but makes coding for
      //	underflow easier.
      // As with IEEE format, negative zero is possible, so
      // we special-case that in hopes that JavaScript will
      // also cooperate.
      //
      // The result is a number in the open interval (-1.0, 1.0);
      // 
      
      // throw away high bits for repeatability.
      rawSflt16 &= 0xFFFF;
   
      // special case minus zero:
      if (rawSflt16 == 0x8000)
          return -0.0;
   
      // extract the sign.
      var sSign = ((rawSflt16 & 0x8000) != 0) ? -1 : 1;
      
      // extract the exponent
      var exp1 = (rawSflt16 >> 11) & 0xF;
   
      // extract the "mantissa" (the fractional part)
      var mant1 = (rawSflt16 & 0x7FF) / 2048.0;
   
      // convert back to a floating point number. We hope 
      // that Math.pow(2, k) is handled efficiently by
      // the JS interpreter! If this is time critical code,
      // you can replace by a suitable shift and divide.
      var f_unscaled = sSign * mant1 * Math.pow(2, exp1 - 15);
   
      return f_unscaled;
  }