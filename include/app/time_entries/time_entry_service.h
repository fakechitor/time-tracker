#pragma once

#include <string>

#include "app/time_entries/time_entry_store.h"

namespace app::time_entries {

class TimeEntryService {
public:
    explicit TimeEntryService(TimeEntryStore& store);

    TimeEntryResult createTimeEntry(const std::string& user_id, const CreateTimeEntryRequest& request);
    TimeEntryResult getTimeEntry(const std::string& user_id, const std::string& entry_id);
    TimeEntriesResult listTimeEntries(const std::string& user_id, const ListTimeEntriesQuery& query);
    TimeEntryResult updateTimeEntry(const std::string& user_id, const std::string& entry_id, const UpdateTimeEntryRequest& request);
    DeleteTimeEntryResult deleteTimeEntry(const std::string& user_id, const std::string& entry_id);

private:
    static bool isNonEmpty(const std::string& value);

    TimeEntryStore& store_;
};

}  // namespace app::time_entries
