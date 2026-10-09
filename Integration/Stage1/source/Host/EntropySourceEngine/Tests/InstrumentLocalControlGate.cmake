file(READ "${SOURCE}" source)
string(REPLACE "if (external_ && cmd.type == SourceCommand::BusMode &&\n            (externalDemandPending_ || producer_.requestPending()))" "if (external_ && !(cmd.type == SourceCommand::Retry && faulted_) &&\n            (externalDemandPending_ || producer_.requestPending()))" source "${source}")
file(WRITE "${OUTPUT}" "${source}")
