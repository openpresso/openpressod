#ifndef BREW_PROFILE_FINGERPRINT_HPP
#define BREW_PROFILE_FINGERPRINT_HPP

#include "utils/fingerprint_encoder.hpp"

#include <cstdint>

#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod::db
{

inline uint64_t getFingerprint(const BrewProfile& profile)
{
  FingerprintEncoder encoder;
  encoder.encode(profile.name());
  encoder.encode(profile.temperature());
  encoder.encode(profile.has_stopcondition());
  if (profile.has_stopcondition()) {
    encoder.encode(profile.stopcondition().type());
    encoder.encode(profile.stopcondition().value());
  }

  encoder.encode(profile.steps_size());
  for (auto&& step : profile.steps()) {
    encoder.encode(step.targettype());
    encoder.encode(step.targetvalue());
    encoder.encode(step.has_advancecondition());
    if (step.has_advancecondition()) {
      encoder.encode(step.advancecondition().type());
      encoder.encode(step.advancecondition().value());
    }
  }

  return encoder.fingerprint();
}

} // namespace openpressod::db

#endif // BREW_PROFILE_FINGERPRINT_HPP