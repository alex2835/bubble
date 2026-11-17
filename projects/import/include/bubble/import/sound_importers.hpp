#pragma once
#include "bubble/import/importer.hpp"

namespace bubble
{
// .wav, .flac and .mp3, kind "sound": the file as it is, once miniaudio
// decodes it. A sound longer than ten seconds - music - streams as it
// plays; a shorter one is decoded whole when it loads. The .import's
// "stream": true or false says otherwise.
vector<Importer> SoundImporters();
}
