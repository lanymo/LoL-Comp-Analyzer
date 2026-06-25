#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

// data/champion_info.json에서 id → 챔피언 이름 매핑을 읽는다.
//
// 범용 JSON 파서가 아니라, 이 데이터셋의 알려진 구조
//   "data": { "1": { "title": ..., "id": 1, "key": ..., "name": "Annie" }, ... }
// 에서 "id" 뒤에 같은 객체의 "name"이 따라온다는 사실만 이용하는 최소 파서다.
// 파일이 없거나 형식이 다르면 빈 맵을 반환하고, 호출부는 id 숫자로 폴백한다.
inline std::unordered_map<int, std::string> loadChampionNames(const std::string& path) {
    std::unordered_map<int, std::string> names;
    std::ifstream f(path);
    if (!f.is_open()) return names;

    std::stringstream ss;
    ss << f.rdbuf();
    const std::string s = ss.str();

    size_t pos = 0;
    while ((pos = s.find("\"id\":", pos)) != std::string::npos) {
        pos += 5;
        int id;
        try { id = std::stoi(s.substr(pos)); }   // 앞 공백은 stoi가 건너뜀
        catch (...) { continue; }

        size_t n = s.find("\"name\":", pos);
        if (n == std::string::npos) break;
        size_t q1 = s.find('"', n + 7);
        size_t q2 = (q1 == std::string::npos) ? std::string::npos : s.find('"', q1 + 1);
        if (q2 == std::string::npos) break;

        names[id] = s.substr(q1 + 1, q2 - q1 - 1);
        pos = q2;
    }
    return names;
}

// 매핑에 없는 id는 "#id"로 표기 
inline std::string champLabel(const std::unordered_map<int, std::string>& names, int id) {
    auto it = names.find(id);
    return it != names.end() ? it->second : ("#" + std::to_string(id));
}

// id 목록을 "Annie, Olaf, Galio" 형태로 합침
template <typename Container>
inline std::string champList(const std::unordered_map<int, std::string>& names,
                             const Container& ids) {
    std::string out;
    for (int id : ids) {
        if (!out.empty()) out += ", ";
        out += champLabel(names, id);
    }
    return out;
}
