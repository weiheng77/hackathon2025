#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <regex>
using namespace std;

// --- Cross-Platform Keyboard Input Setup ---
#ifdef _WIN32
#include <conio.h>
#define ESC_KEY 27
void set_raw_mode() {}
void restore_mode() {}
int get_key_press() { return _kbhit() ? _getch() : 0; }
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#define ESC_KEY 27

struct termios original_terminal_settings;

void set_raw_mode() {
    tcgetattr(STDIN_FILENO, &original_terminal_settings);
    struct termios raw = original_terminal_settings;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void restore_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal_settings);
}

int get_key_press() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval timeout = { 0, 0 };
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout) > 0) {
        char key;
        if (read(STDIN_FILENO, &key, 1) > 0) {
            return static_cast<int>(key);
        }
    }
    return 0;
}
#endif
// -------------------------------------------------------------------------

// Structure to store API data
struct APIData {
    string district;
    string state;
    int apiReading;
    string status;
    string date;
};

class AirPollutantAI {
private:
    map<string, string> knowledge_base_;
    vector<string> default_responses_;
    vector<APIData> api_data_;

public:
    AirPollutantAI() {
        srand(time(0));
        loadAPIData("malaysia_api_1month_daily.txt");
        initializeKnowledgeBase();
    }

    void loadAPIData(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Warning: Could not open file " << filename << endl;
            return;
        }

        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            APIData record;
            size_t pos = 0;
            string token;
            int fieldCount = 0;
            string tempLine = line;

            while ((pos = tempLine.find(',')) != string::npos) {
                token = tempLine.substr(0, pos);
                switch (fieldCount) {
                case 0: record.district = token; break;
                case 1: record.state = token; break;
                case 2:
                    try { record.apiReading = stoi(token); }
                    catch (...) { record.apiReading = 0; }
                    break;
                case 3: record.status = token; break;
                }
                tempLine.erase(0, pos + 1);
                fieldCount++;
            }
            record.date = tempLine;
            api_data_.push_back(record);
        }
        file.close();
        cout << "Loaded " << api_data_.size() << " air quality records.\n";
    }

    void initializeKnowledgeBase() {
        knowledge_base_["hello"] = "Hello! I am Malaysia Air Pollutant AI with 1-month historical data (Oct-Nov 2025).";
        knowledge_base_["hi"] = "Hi! I have daily API data. Ask me about specific dates like 'today', '29 Nov', or 'How was KL yesterday?'";
        knowledge_base_["air quality"] = "I have 1 month of daily API data. Which area or date are you interested in?";
        knowledge_base_["api"] = "API stands for Air Pollutant Index. I can show historical trends since October 2025.";
        knowledge_base_["today"] = "I can show you today's air quality data. Try: 'today api' or 'air quality today'";
        knowledge_base_["29 nov"] = "I have data for November 29th. Try: '29 Nov API data' or 'How was KL on 29 Nov?'";
        knowledge_base_["history"] = "I have data from October 29 to November 29, 2025. Ask about specific dates!";
        knowledge_base_["trend"] = "I can show air quality trends. Try: 'trend in Kuala Lumpur' or 'compare months'";
        knowledge_base_["pollution"] = "I monitor air pollution levels across Malaysia. Try asking about a specific state or district.";
        knowledge_base_["malaysia"] = "I have air quality data for Malaysia. You can ask about states like Selangor, Penang, Johor, etc.";
        knowledge_base_["quit"] = "Thank you for using Malaysia Air Pollutant AI. Breathe easy!";
        knowledge_base_["exit"] = "Thank you for using Malaysia Air Pollutant AI. Stay safe!";

        default_responses_ = {
            "I have daily air quality data. Try: 'today', '29 Nov', or 'How was Kuala Lumpur yesterday?'",
            "Ask me about specific dates like 'today's API' or 'air quality on November 29'",
            "Try: 'Show me data for 29 Nov' or 'How was Selangor today?'",
            "I can show air quality for any date between Oct 29 and Nov 29, 2025",
            "Ask about specific dates and areas like 'Kuala Lumpur on 29 November'"
        };
    }

    string getStatusColor(const string& status) {
        if (status == "Good") return "\033[32m";
        if (status == "Moderate") return "\033[33m";
        if (status == "Unhealthy") return "\033[31m";
        return "\033[0m";
    }

    string generateResponse(const string& user_message) {
        string lower_message = user_message;
        transform(lower_message.begin(), lower_message.end(), lower_message.begin(), ::tolower);

        // First, check for ranking queries
        string ranking_response = getRanking(user_message);
        if (!ranking_response.empty()) {
            return ranking_response;
        }

        // Enhanced health advisory with location detection
        string health_advice = getHealthAdvisoryWithLocation(user_message);
        if (!health_advice.empty()) {
            return health_advice;
        }

        // Check for date-specific queries
        string extracted_date = extractDateFromQuery(user_message);
        if (!extracted_date.empty()) {
            // Check if user is asking about a specific area with date using enhanced matching
            for (const auto& data : api_data_) {
                if (isAreaMatch(user_message, data.district, data.state)) {
                    return getDataForAreaAndDate(data.district, extracted_date);
                }
            }

            // If no specific area, return all data for that date
            return getDataForDate(extracted_date);
        }

        // Check for "today" specifically
        if (lower_message.find("today") != string::npos) {
            if (lower_message.find("api") != string::npos || lower_message.find("air quality") != string::npos) {
                // Check if specific area mentioned with "today"
                for (const auto& data : api_data_) {
                    if (isAreaMatch(user_message, data.district, data.state)) {
                        return getDataForAreaAndDate(data.district, "2025-11-29");
                    }
                }
                return getDataForDate("2025-11-29");
            }
        }

        // Check for historical/temporal queries
        if (lower_message.find("trend") != string::npos) {
            return analyzeTrends(user_message);
        }
        if (lower_message.find("history") != string::npos || lower_message.find("historical") != string::npos) {
            return getHistoricalSummary();
        }
        if (lower_message.find("november") != string::npos || lower_message.find("october") != string::npos) {
            return analyzeByMonth(user_message);
        }
        if (lower_message.find("compare") != string::npos) {
            return compareAreasOrTime(user_message);
        }

        // Check for specific air quality queries
        if (lower_message.find("worst") != string::npos) {
            if (lower_message.find("day") != string::npos || lower_message.find("date") != string::npos) {
                return getWorstDays();
            }
            return getWorstAreas();
        }
        if (lower_message.find("best") != string::npos) {
            if (lower_message.find("day") != string::npos || lower_message.find("date") != string::npos) {
                return getBestDays();
            }
            return getBestAreas();
        }
        if (lower_message.find("list") != string::npos || lower_message.find("all") != string::npos) {
            return getAllAreas();
        }
        if (lower_message.find("stat") != string::npos) {
            return getStatistics();
        }

        // Check for state/district queries with date context - USING ENHANCED MATCHING
        for (const auto& data : api_data_) {
            if (isAreaMatch(user_message, data.district, data.state)) {
                return getAreaInfoWithHistory(data.district, data.state, user_message);
            }
        }

        // Check knowledge base
        for (const auto& pair : knowledge_base_) {
            if (lower_message.find(pair.first) != string::npos) {
                return pair.second;
            }
        }

        return getRandomResponse();
    }

private:
    // Ranking methods
    string getRanking(const string& user_message) {
        string lower_msg = user_message;
        transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        if (lower_msg.find("rank") != string::npos ||
            lower_msg.find("cleanest") != string::npos ||
            lower_msg.find("best") != string::npos) {
            return getCleanestAreasRanking();
        }

        if (lower_msg.find("most polluted") != string::npos ||
            lower_msg.find("worst") != string::npos ||
            lower_msg.find("dirtiest") != string::npos) {
            return getMostPollutedAreasRanking();
        }

        if (lower_msg.find("ranking") != string::npos ||
            lower_msg.find("top") != string::npos ||
            lower_msg.find("list") != string::npos) {
            return getCompleteRanking();
        }

        return "";
    }

    string getCleanestAreasRanking() {
        vector<pair<string, double>> area_avgs = calculateAreaAverages();

        // Sort by average API (ascending - lower is better)
        sort(area_avgs.begin(), area_avgs.end(),
            [](const pair<string, double>& a, const pair<string, double>& b) {
                return a.second < b.second;
            });

        stringstream ss;
        ss << "🏆 CLEANEST AREAS RANKING (Average API - Lower is Better):\n";
        ss << "=============================================\n";

        for (int i = 0; i < min(10, (int)area_avgs.size()); i++) {
            string medal = "";
            if (i == 0) medal = "🥇 ";
            else if (i == 1) medal = "🥈 ";
            else if (i == 2) medal = "🥉 ";
            else medal = to_string(i + 1) + ". ";

            ss << medal << area_avgs[i].first << " - API: " << fixed << setprecision(1) << area_avgs[i].second << "\n";
        }

        return ss.str();
    }

    string getMostPollutedAreasRanking() {
        vector<pair<string, double>> area_avgs = calculateAreaAverages();

        // Sort by average API (descending - higher is worse)
        sort(area_avgs.begin(), area_avgs.end(),
            [](const pair<string, double>& a, const pair<string, double>& b) {
                return a.second > b.second;
            });

        stringstream ss;
        ss << "⚠️ MOST POLLUTED AREAS RANKING (Average API - Higher is Worse):\n";
        ss << "=================================================\n";

        for (int i = 0; i < min(10, (int)area_avgs.size()); i++) {
            string warning = "";
            if (i == 0) warning = "🔴 ";
            else if (i == 1) warning = "🟠 ";
            else if (i == 2) warning = "🟡 ";
            else warning = to_string(i + 1) + ". ";

            ss << warning << area_avgs[i].first << " - API: " << fixed << setprecision(1) << area_avgs[i].second << "\n";
        }

        return ss.str();
    }

    string getCompleteRanking() {
        vector<pair<string, double>> area_avgs = calculateAreaAverages();

        // Sort by average API (ascending)
        sort(area_avgs.begin(), area_avgs.end(),
            [](const pair<string, double>& a, const pair<string, double>& b) {
                return a.second < b.second;
            });

        stringstream ss;
        ss << "📊 COMPLETE AIR QUALITY RANKING:\n";
        ss << "===============================\n";

        for (int i = 0; i < area_avgs.size(); i++) {
            string rank_indicator = to_string(i + 1) + ". ";
            if (i == 0) rank_indicator = "🥇 ";
            else if (i == 1) rank_indicator = "🥈 ";
            else if (i == 2) rank_indicator = "🥉 ";
            else if (i < 10) rank_indicator = to_string(i + 1) + ". ";

            string status = getStatusFromAPI(area_avgs[i].second);
            string color = getStatusColor(status);
            string reset = "\033[0m";

            ss << rank_indicator << area_avgs[i].first << " - API: " << fixed << setprecision(1)
                << area_avgs[i].second << " (" << color << status << reset << ")\n";
        }

        return ss.str();
    }

    vector<pair<string, double>> calculateAreaAverages() {
        map<string, vector<int>> area_readings;

        // Collect all readings for each area
        for (const auto& data : api_data_) {
            string key = data.district + ", " + data.state;
            area_readings[key].push_back(data.apiReading);
        }

        vector<pair<string, double>> averages;
        for (const auto& pair : area_readings) {
            double sum = 0;
            for (int reading : pair.second) sum += reading;
            double avg = sum / pair.second.size();
            averages.push_back({ pair.first, avg });
        }

        return averages;
    }

    string getStatusFromAPI(double api) {
        if (api <= 50) return "Good";
        if (api <= 100) return "Moderate";
        return "Unhealthy";
    }

    // Enhanced health advisory with location prompt
    string getHealthAdvisoryWithLocation(const string& user_message) {
        string lower_msg = user_message;
        transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        // Check for health-related questions
        bool health_question = (lower_msg.find("go out") != string::npos ||
            lower_msg.find("go outside") != string::npos ||
            lower_msg.find("outdoor") != string::npos ||
            lower_msg.find("exercise") != string::npos ||
            lower_msg.find("workout") != string::npos ||
            lower_msg.find("jog") != string::npos ||
            lower_msg.find("run") != string::npos ||
            lower_msg.find("walk") != string::npos ||
            lower_msg.find("healthy") != string::npos ||
            lower_msg.find("safe") != string::npos ||
            lower_msg.find("haze") != string::npos);

        if (!health_question) {
            return "";
        }

        // Check if user mentioned a specific location
        string detected_location = detectLocationInQuery(user_message);

        if (detected_location.empty()) {
            return "🤔 I'd be happy to advise you about going out! But first, could you tell me which area you're in? "
                "For example: 'Kuala Lumpur', 'Selangor', 'Penang', etc. This will help me give you more accurate advice based on local air quality.";
        }

        // If location is detected, proceed with specific advice
        return getSpecificHealthAdvisory(detected_location);
    }

    string detectLocationInQuery(const string& user_message) {
        string lower_msg = user_message;
        transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        // Common Malaysian locations and their variations
        vector<string> locations = {
            "kuala lumpur", "kl", "selangor", "penang", "johor", "johor bahru", "jb",
            "ipoh", "kuching", "kota kinabalu", "kk", "malacca", "melaka",
            "seremban", "alor setar", "kuala terengganu", "kota bharu",
            "shah alam", "petaling jaya", "pj", "subang", "puchong"
        };

        for (const string& location : locations) {
            if (lower_msg.find(location) != string::npos) {
                return location;
            }
        }

        return "";
    }

    string getSpecificHealthAdvisory(const string& location) {
        // Get today's data for the specified location
        vector<APIData> today_location_data;

        for (const auto& data : api_data_) {
            if (data.date == "2025-11-29") {
                string lower_district = data.district;
                string lower_state = data.state;
                string lower_location = location;

                transform(lower_district.begin(), lower_district.end(), lower_district.begin(), ::tolower);
                transform(lower_state.begin(), lower_state.end(), lower_state.begin(), ::tolower);
                transform(lower_location.begin(), lower_location.end(), lower_location.begin(), ::tolower);

                if (lower_district.find(lower_location) != string::npos ||
                    lower_state.find(lower_location) != string::npos ||
                    (lower_location == "kl" && lower_district.find("kuala lumpur") != string::npos) ||
                    (lower_location == "jb" && lower_district.find("johor bahru") != string::npos) ||
                    (lower_location == "kk" && lower_district.find("kota kinabalu") != string::npos)) {

                    today_location_data.push_back(data);
                }
            }
        }

        if (today_location_data.empty()) {
            return "I couldn't find specific air quality data for " + location + " today. "
                "You can check the overall Malaysia air quality or try asking about a nearby major city.";
        }

        stringstream ss;
        ss << "📍 Health Advisory for " << location << " (Today - 29 Nov 2025):\n";
        ss << "================================\n\n";

        for (const auto& data : today_location_data) {
            string color = getStatusColor(data.status);
            string reset = "\033[0m";

            ss << "🏙️  " << data.district << ", " << data.state << "\n";
            ss << "📊 API: " << data.apiReading << " (" << color << data.status << reset << ")\n\n";

            // Detailed health advice based on API level
            if (data.apiReading <= 50) {
                ss << "✅ EXCELLENT CONDITIONS - GO OUTSIDE! 🌞\n";
                ss << "• Perfect for all outdoor activities\n";
                ss << "• Great day for exercise, sports, and recreation\n";
                ss << "• Enjoy the fresh air safely\n";
            }
            else if (data.apiReading <= 100) {
                ss << "⚠️ MODERATE CONDITIONS - PROCEED WITH CAUTION\n";
                ss << "• Generally acceptable for most people\n";
                ss << "• Unusually sensitive individuals should reduce prolonged outdoor exertion\n";
                ss << "• Good for light activities like walking\n";
                ss << "• Consider shorter outdoor sessions\n";
            }
            else {
                ss << "❌ UNHEALTHY CONDITIONS - LIMIT OUTDOOR TIME\n";
                ss << "• Everyone may begin to experience health effects\n";
                ss << "• Sensitive groups should avoid outdoor activities\n";
                ss << "• If you must go out, keep it brief\n";
                ss << "• Avoid strenuous exercise outdoors\n";
                ss << "• Consider indoor alternatives\n";
            }

            ss << "\n";
        }

        // Add general tips
        ss << "💡 General Tips:\n";
        ss << "• Check air quality before planning outdoor activities\n";
        ss << "• Sensitive groups include children, elderly, and people with respiratory conditions\n";
        ss << "• Use air purifiers indoors if air quality is poor\n";
        ss << "• Stay hydrated and listen to your body\n";

        return ss.str();
    }

    // Simple and reliable area detection method
    bool isAreaMatch(const string& user_message, const string& district, const string& state) {
        string lower_msg = user_message;
        transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        string lower_district = district;
        string lower_state = state;
        transform(lower_district.begin(), lower_district.end(), lower_district.begin(), ::tolower);
        transform(lower_state.begin(), lower_state.end(), lower_state.begin(), ::tolower);

        // Direct matches
        if (lower_msg.find(lower_district) != string::npos) return true;
        if (lower_msg.find(lower_state) != string::npos) return true;

        // Common abbreviations
        if (lower_msg.find("jb") != string::npos && lower_district.find("johor bahru") != string::npos) return true;
        if (lower_msg.find("kl") != string::npos && lower_district.find("kuala lumpur") != string::npos) return true;
        if (lower_msg.find("melaka") != string::npos && lower_state.find("malacca") != string::npos) return true;
        if (lower_msg.find("malacca") != string::npos && lower_state.find("malacca") != string::npos) return true;
        if (lower_msg.find("penang") != string::npos && lower_state.find("penang") != string::npos) return true;
        if (lower_msg.find("kk") != string::npos && lower_district.find("kota kinabalu") != string::npos) return true;

        return false;
    }

    // Date handling methods
    string extractDateFromQuery(const string& user_message) {
        string lower_msg = user_message;
        transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        // Map month names to numbers
        map<string, string> months = {
            {"jan", "01"}, {"january", "01"},
            {"feb", "02"}, {"february", "02"},
            {"mar", "03"}, {"march", "03"},
            {"apr", "04"}, {"april", "04"},
            {"may", "05"},
            {"jun", "06"}, {"june", "06"},
            {"jul", "07"}, {"july", "07"},
            {"aug", "08"}, {"august", "08"},
            {"sep", "09"}, {"september", "09"},
            {"oct", "10"}, {"october", "10"},
            {"nov", "11"}, {"november", "11"},
            {"dec", "12"}, {"december", "12"}
        };

        // Check for specific date patterns
        if (lower_msg.find("today") != string::npos) {
            return "2025-11-29";
        }
        if (lower_msg.find("yesterday") != string::npos) {
            return "2025-11-28";
        }

        // Check for "29 nov" pattern
        regex date_pattern1(R"((\d{1,2})\s*(jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec))", regex_constants::icase);
        smatch match;
        if (regex_search(user_message, match, date_pattern1)) {
            string day = match[1];
            string month = match[2];
            transform(month.begin(), month.end(), month.begin(), ::tolower);

            if (day.length() == 1) day = "0" + day;
            if (months.find(month) != months.end()) {
                return "2025-" + months[month] + "-" + day;
            }
        }

        // Check for "nov 29" pattern
        regex date_pattern2(R"((jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)\s*(\d{1,2}))", regex_constants::icase);
        if (regex_search(user_message, match, date_pattern2)) {
            string month = match[1];
            string day = match[2];
            transform(month.begin(), month.end(), month.begin(), ::tolower);

            if (day.length() == 1) day = "0" + day;
            if (months.find(month) != months.end()) {
                return "2025-" + months[month] + "-" + day;
            }
        }

        return "";
    }

    string getDataForDate(const string& date) {
        vector<APIData> date_data;
        for (const auto& data : api_data_) {
            if (data.date == date) {
                date_data.push_back(data);
            }
        }

        if (date_data.empty()) {
            return "No data available for " + date;
        }

        stringstream ss;
        ss << "Air Quality Data for " << date << ":\n";
        ss << "================================\n";

        // Group by state
        map<string, vector<APIData>> state_data;
        for (const auto& data : date_data) {
            state_data[data.state].push_back(data);
        }

        for (const auto& state_pair : state_data) {
            ss << "\n" << state_pair.first << ":\n";
            for (const auto& data : state_pair.second) {
                string color_code = getStatusColor(data.status);
                string reset_code = "\033[0m";
                ss << "  • " << data.district << " - API: " << data.apiReading
                    << " (" << color_code << data.status << reset_code << ")\n";
            }
        }

        // Add summary
        double avg = 0;
        int worst = 0, best = 1000;
        string worst_area, best_area;

        for (const auto& data : date_data) {
            avg += data.apiReading;
            if (data.apiReading > worst) {
                worst = data.apiReading;
                worst_area = data.district + ", " + data.state;
            }
            if (data.apiReading < best) {
                best = data.apiReading;
                best_area = data.district + ", " + data.state;
            }
        }
        avg /= date_data.size();

        ss << "\nSummary for " << date << ":\n";
        ss << "• Average API: " << fixed << setprecision(1) << avg << "\n";
        ss << "• Worst: " << worst_area << " (API: " << worst << ")\n";
        ss << "• Best: " << best_area << " (API: " << best << ")\n";
        ss << "• Areas monitored: " << date_data.size() << "\n";

        return ss.str();
    }

    string getDataForAreaAndDate(const string& area, const string& date) {
        for (const auto& data : api_data_) {
            string lower_district = data.district;
            transform(lower_district.begin(), lower_district.end(), lower_district.begin(), ::tolower);
            string lower_state = data.state;
            transform(lower_state.begin(), lower_state.end(), lower_state.begin(), ::tolower);
            string lower_area = area;
            transform(lower_area.begin(), lower_area.end(), lower_area.begin(), ::tolower);

            if ((lower_district.find(lower_area) != string::npos ||
                lower_state.find(lower_area) != string::npos) &&
                data.date == date) {

                stringstream ss;
                ss << "Air Quality in " << data.district << ", " << data.state << " on " << date << ":\n";
                ss << "• API Reading: " << data.apiReading << "\n";
                ss << "• Status: " << data.status << "\n";
                ss << "• Advice: " << getHealthAdvice(data.status) << "\n";

                // Add context - compare with previous day if available
                string prev_date = getPreviousDate(date);
                if (!prev_date.empty()) {
                    for (const auto& prev_data : api_data_) {
                        if (prev_data.district == data.district && prev_data.state == data.state &&
                            prev_data.date == prev_date) {
                            int change = data.apiReading - prev_data.apiReading;
                            string trend = change > 0 ? "worsened" : (change < 0 ? "improved" : "stable");
                            ss << "• Change from previous day: " << trend << " by " << abs(change) << " points\n";
                            break;
                        }
                    }
                }

                return ss.str();
            }
        }
        return "No data found for " + area + " on " + date;
    }

    string getPreviousDate(const string& date) {
        // Simple date decrement for our dataset
        if (date == "2025-11-29") return "2025-11-28";
        if (date == "2025-11-28") return "2025-11-27";
        if (date == "2025-11-27") return "2025-11-26";
        if (date == "2025-11-26") return "2025-11-25";
        if (date == "2025-11-25") return "2025-11-24";
        if (date == "2025-11-24") return "2025-11-23";
        if (date == "2025-11-23") return "2025-11-22";
        if (date == "2025-11-22") return "2025-11-21";
        if (date == "2025-11-21") return "2025-11-20";
        if (date == "2025-11-20") return "2025-11-19";
        if (date == "2025-11-19") return "2025-11-18";
        if (date == "2025-11-18") return "2025-11-17";
        if (date == "2025-11-17") return "2025-11-16";
        if (date == "2025-11-16") return "2025-11-15";
        if (date == "2025-11-15") return "2025-11-14";
        if (date == "2025-11-14") return "2025-11-13";
        if (date == "2025-11-13") return "2025-11-12";
        if (date == "2025-11-12") return "2025-11-11";
        if (date == "2025-11-11") return "2025-11-10";
        if (date == "2025-11-10") return "2025-11-09";
        if (date == "2025-11-09") return "2025-11-08";
        if (date == "2025-11-08") return "2025-11-07";
        if (date == "2025-11-07") return "2025-11-06";
        if (date == "2025-11-06") return "2025-11-05";
        if (date == "2025-11-05") return "2025-11-04";
        if (date == "2025-11-04") return "2025-11-03";
        if (date == "2025-11-03") return "2025-11-02";
        if (date == "2025-11-02") return "2025-11-01";
        if (date == "2025-11-01") return "2025-10-31";
        if (date == "2025-10-31") return "2025-10-30";
        if (date == "2025-10-30") return "2025-10-29";
        return "";
    }

    // Health advice method
    string getHealthAdvice(const string& status) {
        if (status == "Good") return "Air quality is satisfactory. Enjoy outdoor activities!";
        if (status == "Moderate") return "Air quality is acceptable. Sensitive people should reduce prolonged outdoor exertion.";
        if (status == "Unhealthy") return "Everyone may experience health effects. Reduce outdoor activities.";
        return "No specific advice available.";
    }

    // Existing methods
    string getWorstAreas() {
        if (api_data_.empty()) return "No data available.";

        // Get latest data for each district
        map<string, APIData> latest_data;
        for (const auto& data : api_data_) {
            if (latest_data.find(data.district) == latest_data.end() ||
                data.date > latest_data[data.district].date) {
                latest_data[data.district] = data;
            }
        }

        vector<APIData> sorted_data;
        for (const auto& pair : latest_data) {
            sorted_data.push_back(pair.second);
        }

        sort(sorted_data.begin(), sorted_data.end(),
            [](const APIData& a, const APIData& b) { return a.apiReading > b.apiReading; });

        stringstream ss;
        ss << "Current worst air quality areas:\n";
        for (int i = 0; i < min(5, (int)sorted_data.size()); i++) {
            ss << "• " << sorted_data[i].district << ", " << sorted_data[i].state
                << " - API: " << sorted_data[i].apiReading << " (" << sorted_data[i].status << ") on " << sorted_data[i].date << "\n";
        }
        return ss.str();
    }

    string getBestAreas() {
        if (api_data_.empty()) return "No data available.";

        map<string, APIData> latest_data;
        for (const auto& data : api_data_) {
            if (latest_data.find(data.district) == latest_data.end() ||
                data.date > latest_data[data.district].date) {
                latest_data[data.district] = data;
            }
        }

        vector<APIData> sorted_data;
        for (const auto& pair : latest_data) {
            sorted_data.push_back(pair.second);
        }

        sort(sorted_data.begin(), sorted_data.end(),
            [](const APIData& a, const APIData& b) { return a.apiReading < b.apiReading; });

        stringstream ss;
        ss << "Current best air quality areas:\n";
        for (int i = 0; i < min(5, (int)sorted_data.size()); i++) {
            ss << "• " << sorted_data[i].district << ", " << sorted_data[i].state
                << " - API: " << sorted_data[i].apiReading << " (" << sorted_data[i].status << ") on " << sorted_data[i].date << "\n";
        }
        return ss.str();
    }

    string getWorstDays() {
        if (api_data_.empty()) return "No data available.";

        vector<APIData> sorted_data = api_data_;
        sort(sorted_data.begin(), sorted_data.end(),
            [](const APIData& a, const APIData& b) { return a.apiReading > b.apiReading; });

        stringstream ss;
        ss << "Worst air quality days recorded:\n";
        for (int i = 0; i < min(5, (int)sorted_data.size()); i++) {
            ss << "• " << sorted_data[i].date << " - " << sorted_data[i].district << ", " << sorted_data[i].state
                << " - API: " << sorted_data[i].apiReading << " (" << sorted_data[i].status << ")\n";
        }
        return ss.str();
    }

    string getBestDays() {
        if (api_data_.empty()) return "No data available.";

        vector<APIData> sorted_data = api_data_;
        sort(sorted_data.begin(), sorted_data.end(),
            [](const APIData& a, const APIData& b) { return a.apiReading < b.apiReading; });

        stringstream ss;
        ss << "Best air quality days recorded:\n";
        for (int i = 0; i < min(5, (int)sorted_data.size()); i++) {
            ss << "• " << sorted_data[i].date << " - " << sorted_data[i].district << ", " << sorted_data[i].state
                << " - API: " << sorted_data[i].apiReading << " (" << sorted_data[i].status << ")\n";
        }
        return ss.str();
    }

    string getAllAreas() {
        if (api_data_.empty()) return "No data available.";

        map<string, APIData> latest_data;
        for (const auto& data : api_data_) {
            if (latest_data.find(data.district) == latest_data.end() ||
                data.date > latest_data[data.district].date) {
                latest_data[data.district] = data;
            }
        }

        stringstream ss;
        ss << "All monitored areas (latest readings):\n";
        for (const auto& pair : latest_data) {
            ss << "• " << pair.second.district << ", " << pair.second.state
                << " - API: " << pair.second.apiReading << " (" << pair.second.status << ") on " << pair.second.date << "\n";
        }
        return ss.str();
    }

    string getAreaInfoWithHistory(const string& district, const string& state, const string& user_message) {
        vector<APIData> area_data;
        for (const auto& data : api_data_) {
            if (data.district == district && data.state == state) {
                area_data.push_back(data);
            }
        }

        if (area_data.empty()) return "Sorry, I couldn't find data for " + district + ", " + state;

        // Sort by date
        sort(area_data.begin(), area_data.end(),
            [](const APIData& a, const APIData& b) { return a.date > b.date; });

        stringstream ss;
        ss << "Air Quality History for " << district << ", " << state << ":\n";

        // Show latest reading
        ss << "Latest (" << area_data[0].date << "): API " << area_data[0].apiReading << " (" << area_data[0].status << ")\n\n";

        // Show trend
        if (area_data.size() >= 2) {
            int change = area_data[0].apiReading - area_data[1].apiReading;
            string trend = change > 0 ? "worsened" : (change < 0 ? "improved" : "stable");
            ss << "Trend: " << trend << " by " << abs(change) << " points from previous day\n\n";
        }

        // Show last 5 days
        ss << "Last 5 days:\n";
        for (int i = 0; i < min(5, (int)area_data.size()); i++) {
            ss << "• " << area_data[i].date << " - API: " << area_data[i].apiReading << " (" << area_data[i].status << ")\n";
        }

        ss << "\nAdvice: " << getHealthAdvice(area_data[0].status);
        return ss.str();
    }

    string analyzeTrends(const string& user_message) {
        string lower_msg = user_message;
        transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        // Simple trend analysis - compare first and last week
        vector<APIData> early_nov, late_nov;

        for (const auto& data : api_data_) {
            if (data.date.find("2025-11-01") != string::npos ||
                data.date.find("2025-11-02") != string::npos ||
                data.date.find("2025-11-03") != string::npos) {
                early_nov.push_back(data);
            }
            if (data.date.find("2025-11-27") != string::npos ||
                data.date.find("2025-11-28") != string::npos ||
                data.date.find("2025-11-29") != string::npos) {
                late_nov.push_back(data);
            }
        }

        if (early_nov.empty() || late_nov.empty()) {
            return "Not enough data for trend analysis.";
        }

        // Calculate averages
        double early_avg = 0, late_avg = 0;
        for (const auto& data : early_nov) early_avg += data.apiReading;
        for (const auto& data : late_nov) late_avg += data.apiReading;
        early_avg /= early_nov.size();
        late_avg /= late_nov.size();

        stringstream ss;
        ss << "Air Quality Trend Analysis (Early vs Late November):\n";
        ss << "• Early Nov (1st-3rd): Average API " << fixed << setprecision(1) << early_avg << "\n";
        ss << "• Late Nov (27th-29th): Average API " << fixed << setprecision(1) << late_avg << "\n";

        double change = late_avg - early_avg;
        if (change > 5) ss << "• Overall: Air quality has worsened\n";
        else if (change < -5) ss << "• Overall: Air quality has improved\n";
        else ss << "• Overall: Air quality remained relatively stable\n";

        return ss.str();
    }

    string getHistoricalSummary() {
        if (api_data_.empty()) return "No data available.";

        stringstream ss;
        ss << "Historical Data Summary (Oct 29 - Nov 29, 2025):\n";
        ss << "• Total records: " << api_data_.size() << "\n";
        ss << "• Monitoring period: 32 days\n";
        ss << "• Districts covered: " << countUniqueDistricts() << "\n";
        ss << "• Data points per district: " << api_data_.size() / countUniqueDistricts() << "\n";
        ss << "\nAsk me about specific dates, trends, or comparisons!";

        return ss.str();
    }

    string analyzeByMonth(const string& user_message) {
        string lower_msg = user_message;
        transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        vector<APIData> october_data, november_data;

        for (const auto& data : api_data_) {
            if (data.date.find("2025-10") != string::npos) {
                october_data.push_back(data);
            }
            else if (data.date.find("2025-11") != string::npos) {
                november_data.push_back(data);
            }
        }

        stringstream ss;

        if (lower_msg.find("october") != string::npos) {
            if (october_data.empty()) {
                ss << "Limited October data available (only 3 days).\n";
            }
            else {
                double avg = 0;
                for (const auto& data : october_data) avg += data.apiReading;
                avg /= october_data.size();

                ss << "October 2025 Analysis (3 days):\n";
                ss << "• Average API: " << fixed << setprecision(1) << avg << "\n";
                ss << "• Days recorded: " << october_data.size() << "\n";
                ss << "• Generally showed higher pollution levels\n";
            }
        }

        if (lower_msg.find("november") != string::npos) {
            double avg = 0;
            for (const auto& data : november_data) avg += data.apiReading;
            avg /= november_data.size();

            ss << "November 2025 Analysis (29 days):\n";
            ss << "• Average API: " << fixed << setprecision(1) << avg << "\n";
            ss << "• Days recorded: " << november_data.size() << "\n";
            ss << "• Showed improving trend throughout the month\n";
        }

        return ss.str();
    }

    string compareAreasOrTime(const string& user_message) {
        // Simple comparison - show top 5 areas by average
        map<string, vector<int>> area_readings;

        for (const auto& data : api_data_) {
            string key = data.district + "," + data.state;
            area_readings[key].push_back(data.apiReading);
        }

        vector<pair<string, double>> averages;
        for (const auto& pair : area_readings) {
            double sum = 0;
            for (int reading : pair.second) sum += reading;
            averages.push_back({ pair.first, sum / pair.second.size() });
        }

        sort(averages.begin(), averages.end(),
            [](const pair<string, double>& a, const pair<string, double>& b) { return a.second > b.second; });

        stringstream ss;
        ss << "Area Comparison (Average API Nov 2025):\n";
        for (int i = 0; i < min(5, (int)averages.size()); i++) {
            ss << "• " << averages[i].first << ": " << fixed << setprecision(1) << averages[i].second << "\n";
        }

        return ss.str();
    }

    string getStatistics() {
        if (api_data_.empty()) return "No data available.";

        int total = 0;
        int good = 0, moderate = 0, unhealthy = 0;
        int max_api = 0, min_api = 1000;

        for (const auto& data : api_data_) {
            total += data.apiReading;
            if (data.status == "Good") good++;
            else if (data.status == "Moderate") moderate++;
            else if (data.status == "Unhealthy") unhealthy++;

            if (data.apiReading > max_api) max_api = data.apiReading;
            if (data.apiReading < min_api) min_api = data.apiReading;
        }

        double average = static_cast<double>(total) / api_data_.size();

        stringstream ss;
        ss << "Malaysia Air Quality Statistics (Oct 29 - Nov 29):\n";
        ss << "• Total records: " << api_data_.size() << "\n";
        ss << "• Districts monitored: " << countUniqueDistricts() << "\n";
        ss << "• Average API: " << fixed << setprecision(1) << average << "\n";
        ss << "• Highest API: " << max_api << "\n";
        ss << "• Lowest API: " << min_api << "\n";
        ss << "• Good: " << good << " readings\n";
        ss << "• Moderate: " << moderate << " readings\n";
        ss << "• Unhealthy: " << unhealthy << " readings";
        return ss.str();
    }

    int countUniqueDistricts() {
        map<string, bool> districts;
        for (const auto& data : api_data_) {
            districts[data.district + data.state] = true;
        }
        return districts.size();
    }

    string getRandomResponse() {
        int index = rand() % default_responses_.size();
        return default_responses_[index];
    }
};

void printHeader() {
    cout << "\n======================================================\n";
    cout << "    MALAYSIA AIR POLLUTANT AI - HISTORICAL DATA    \n";
    cout << "======================================================\n";
    cout << "I have 1 month of daily API data (Oct 29 - Nov 29, 2025)!\n";
    cout << "Try asking about:\n";
    cout << "- Specific dates: 'today', '29 Nov', 'yesterday'\n";
    cout << "- Areas with dates: 'KL today', 'Selangor on 29 Nov', 'melaka today'\n";
    cout << "- Health advice: 'can I go out today?', 'is it safe to exercise in KL?'\n";
    cout << "- Rankings: 'cleanest areas', 'most polluted ranking', 'top 10'\n";
    cout << "- Trends and comparisons\n";
    cout << "Press ESC at any time to exit.\n\n";
}

int main() {
    atexit(restore_mode);
    set_raw_mode();

    AirPollutantAI bot;
    printHeader();

    string user_input = "";
    bool running = true;

    cout << "You: ";

    while (running) {
        int key = get_key_press();

        if (key == ESC_KEY) {
            running = false;
            break;
        }
        else if (key != 0) {
            if (key == '\n' || key == '\r') {
                cout << "\n";
                if (user_input.empty()) {
                    cout << "You: ";
                    continue;
                }

                string ai_response = bot.generateResponse(user_input);
                cout << "AI: " << ai_response << "\n\n";
                user_input = "";
                cout << "You: ";
            }
            else if (key == 127 || key == 8) {
                if (!user_input.empty()) {
                    user_input.pop_back();
                    cout << "\b \b";
                }
            }
            else if (key >= 32 && key <= 126) {
                char c = static_cast<char>(key);
                user_input += c;
                cout << c;
            }
        }

#ifndef _WIN32
        usleep(10000);
#endif
    }

    restore_mode();
    cout << "\nAI: " << bot.generateResponse("quit") << "\n";
    cout << "\n======================================================\n";
    cout << "           Thank you for using our service!           \n";
    cout << "======================================================\n";

    return 0;
}