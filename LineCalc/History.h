#pragma once
#include <string>
#include <vector>

class History
{
public:
    void submit(const std::wstring& text)
    {
        entries.push_back(text);
        browsing = false;
        position = entries.size();
    }
    std::wstring move(const std::wstring& current, bool up, bool saveCurrent)
    {
        if (!browsing)
        {
            position = entries.size();
            if (saveCurrent && !current.empty() && (entries.empty() || entries.back() != current))
            {
                entries.push_back(current);
                position = entries.size() - 1;
            }
            browsing = true;
        }
        else if (saveCurrent && !current.empty() && current != recalled) entries.push_back(current);
        if (up) { if (position > 0) --position; }
        else if (position < entries.size()) ++position;
        recalled = position < entries.size() ? entries[position] : L"";
        return recalled;
    }
    void clear()
    {
        entries.clear();
        position = 0;
        browsing = false;
        recalled.clear();
    }
    size_t size() const { return entries.size(); }
private:
    std::vector<std::wstring> entries;
    size_t position = 0;
    bool browsing = false;
    std::wstring recalled;
};
