#include "ClapProcessor.h"

// clap-helpers keeps its template method definitions in .hxx files that must
// be compiled into exactly one TU. Doing it here — with an explicit
// instantiation for the checking level ClapProcessor fixes — means every
// plugin that links imagiro_processor_clap gets the Plugin/HostProxy symbols
// for free, instead of each plugin remembering to include both .hxx files.
#include <clap/helpers/plugin.hxx>
#include <clap/helpers/host-proxy.hxx>

namespace clap::helpers {
    template class Plugin<MisbehaviourHandler::Terminate, CheckingLevel::Maximal>;
    template class HostProxy<MisbehaviourHandler::Terminate, CheckingLevel::Maximal>;
}
