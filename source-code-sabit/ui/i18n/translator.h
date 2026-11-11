#ifndef SABIT_UI_TRANSLATOR_H
#define SABIT_UI_TRANSLATOR_H

#include <string>
#include <map>
#include <QString>

namespace sabit::ui {

enum class Language {
    Indonesian,
    English
};

class Translator {
public:
    static Translator& instance();

    void set_language(Language lang);
    [[nodiscard]] Language language() const { return current_lang_; }

    [[nodiscard]] std::string tr_std(const std::string& key) const;
    [[nodiscard]] QString tr(const std::string& key) const;

private:
    Translator();
    void init_dictionary();

    Language current_lang_{Language::English};
    std::map<std::string, std::string> id_dict_;
    std::map<std::string, std::string> en_dict_;
};

inline QString tr_str(const std::string& key) {
    return Translator::instance().tr(key);
}

inline std::string tr_str_std(const std::string& key) {
    return Translator::instance().tr_std(key);
}

} // namespace sabit::ui

#endif // SABIT_UI_TRANSLATOR_H
