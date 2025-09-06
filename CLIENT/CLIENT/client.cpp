#include "stdafx.h"

struct attack_effect
{
    sf::Sprite sprite;
    sf::Time end_time;
};

sf::TcpSocket s_socket;
sf::RenderWindow* g_window;
sf::Font g_font;

static sf::Texture self_texture;
static sf::Texture other_texture;
static sf::Texture randommonster_texture;
static sf::Texture followmonster_texture;

static sf::Texture obstacle_texture;
static sf::Sprite  obstacle_sprite;

static sf::Texture grass_texture;
static sf::Sprite  grass_sprite;

static std::string my_name;

static sf::Texture attack_texture;
static std::vector<attack_effect> attack_effects;

constexpr auto SCREEN_WIDTH = 16;
constexpr auto SCREEN_HEIGHT = 16;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH;
constexpr auto WINDOW_HEIGHT = SCREEN_HEIGHT * TILE_WIDTH;

constexpr size_t CHAT_HISTORY_MAX = 10;

int g_left_x;
int g_top_y;
int g_myid;

bool inventory_open = false;
static bool is_typing = false;

enum GameState 
{
    LOGIN_SCREEN,
    GAME_RUNNING
};

static GameState current_state = LOGIN_SCREEN;
static bool is_entering_ip = true;
static sf::String ip_string;
static sf::String id_string;
static sf::Text login_title_text;
static sf::Text ip_label_text;
static sf::Text id_label_text;
static sf::Text ip_input_text;
static sf::Text id_input_text;
static sf::Text instruction_text;
static sf::Text error_text;
static sf::RectangleShape ip_input_box;
static sf::RectangleShape id_input_box;
static std::string error_message;

static bool shop_open = false;
static sf::RectangleShape shop_panel;
static sf::RectangleShape hp_button;
static sf::RectangleShape exp_button;
static sf::Text shop_title_text;
static sf::Text shop_desc_text;
static sf::Text hp_label_text;
static sf::Text exp_label_text;
static sf::Text gold_label_text;
static sf::FloatRect hp_btn_bounds;
static sf::FloatRect exp_btn_bounds;

static bool attendance_open = false;
static sf::RectangleShape attend_panel;
static sf::RectangleShape attend_button;
static sf::Text attend_title_text;
static sf::Text attend_desc_text;
static sf::Text attend_btn_text;
static sf::FloatRect attend_btn_bounds;

static std::deque<std::string> chat_history;
static std::unordered_set<int> obstacle_set;

static sf::String chat_string;
static sf::Text chat_input_text;

static sf::Clock game_clock;
sf::Time now = game_clock.getElapsedTime();

class OBJECT
{
private:
    bool m_showing;

    sf::Sprite m_sprite;
    sf::Text m_name;
    sf::Text m_chat;

    std::chrono::system_clock::time_point m_mess_end_time;

public:
    int id;
    int m_x, m_y;
    int m_hp;
    int m_exp;
    int m_max_hp;
    int m_level;
    int m_potion;
    int m_exppotion;
    int m_gold;
    char name[NAME_SIZE];

    OBJECT(sf::Texture& t, int x, int y, int x2, int y2)
    {
        m_showing = false;
        m_sprite.setTexture(t);
        m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
        name[0] = '\0';
        set_name("NONAME");
        m_mess_end_time = std::chrono::system_clock::now();
    }

    OBJECT()
    {
        m_showing = false;
        name[0] = '\0';
    }

    void show()
    {
        m_showing = true;
    }

    void hide()
    {
        m_showing = false;
    }

    void a_move(int x, int y)
    {
        m_sprite.setPosition((float)x, (float)y);
    }

    void a_draw()
    {
        g_window->draw(m_sprite);
    }

    void move(int x, int y)
    {
        m_x = x;
        m_y = y;
    }

    void draw()
    {
        if (!m_showing)
        {
            return;
        }

        float rx = (m_x - g_left_x) * 65.0f + 1;
        float ry = (m_y - g_top_y) * 65.0f + 1;
        m_sprite.setPosition(rx, ry);
        g_window->draw(m_sprite);

        auto size = m_name.getGlobalBounds();

        if (m_mess_end_time < std::chrono::system_clock::now())
        {
            m_name.setPosition(rx + 32 - size.width / 2, ry - 10);
            g_window->draw(m_name);
        }

        else
        {
            m_chat.setPosition(rx + 32 - size.width / 2, ry - 10);
            g_window->draw(m_chat);
        }
    }

    void set_stat(int hp, int max_hp, int exp, int level, int potion, int exp_potion, int gold)
    {
        m_hp = hp;
        m_max_hp = max_hp;
        m_exp = exp;
        m_level = level;
        m_potion = potion;
        m_exppotion = exp_potion;
        m_gold = gold;
    }

    void set_name(const char str[])
    {
        strncpy_s(name, NAME_SIZE, str, _TRUNCATE);
        name[NAME_SIZE - 1] = '\0';

        m_name.setFont(g_font);
        m_name.setString(str);

        if (id < MAX_USER)
        {
            m_name.setFillColor(sf::Color(255, 255, 255));
        }

        else
        {
            m_name.setFillColor(sf::Color(255, 255, 0));
        }

        m_name.setStyle(sf::Text::Bold);
    }

    void set_chat(const char* utf8str)
    {
        sf::String ustr = sf::String::fromUtf8((const sf::Uint8*)utf8str, (const sf::Uint8*)(utf8str + std::strlen(utf8str)));

        m_chat.setFont(g_font);
        m_chat.setString(ustr);
        m_chat.setFillColor(sf::Color::White);
        m_chat.setStyle(sf::Text::Bold);
        m_mess_end_time = std::chrono::system_clock::now() + std::chrono::seconds(3);
    }
};

OBJECT avatar;
std::unordered_map<int, OBJECT> players;

sf::Texture* board;
sf::Texture* pieces;

void initialize_login_screen()
{
    login_title_text.setFont(g_font);
    login_title_text.setString("=== GAME LOGIN ===");
    login_title_text.setCharacterSize(32);
    login_title_text.setFillColor(sf::Color::White);
    login_title_text.setStyle(sf::Text::Bold);
    auto title_bounds = login_title_text.getLocalBounds();
    login_title_text.setPosition(WINDOW_WIDTH / 2 - title_bounds.width / 2, 100);

    ip_label_text.setFont(g_font);
    ip_label_text.setString("Server IP:");
    ip_label_text.setCharacterSize(20);
    ip_label_text.setFillColor(sf::Color::White);
    ip_label_text.setPosition(200, 200);

    id_label_text.setFont(g_font);
    id_label_text.setString("User ID (Max 10 chars):");
    id_label_text.setCharacterSize(20);
    id_label_text.setFillColor(sf::Color::White);
    id_label_text.setPosition(200, 300);

    ip_input_box.setSize(sf::Vector2f(400, 30));
    ip_input_box.setPosition(200, 230);
    ip_input_box.setFillColor(sf::Color(50, 50, 50));
    ip_input_box.setOutlineThickness(2);
    ip_input_box.setOutlineColor(sf::Color::Green);

    id_input_box.setSize(sf::Vector2f(400, 30));
    id_input_box.setPosition(200, 330);
    id_input_box.setFillColor(sf::Color(50, 50, 50));
    id_input_box.setOutlineThickness(2);
    id_input_box.setOutlineColor(sf::Color::White);

    ip_input_text.setFont(g_font);
    ip_input_text.setCharacterSize(18);
    ip_input_text.setFillColor(sf::Color::White);
    ip_input_text.setPosition(205, 235);

    id_input_text.setFont(g_font);
    id_input_text.setCharacterSize(18);
    id_input_text.setFillColor(sf::Color::White);
    id_input_text.setPosition(205, 335);

    instruction_text.setFont(g_font);
    instruction_text.setString(
        "Enter Server IP first, then press TAB to switch to ID input.\nPress ENTER to connect when both fields are filled."
    );
    instruction_text.setCharacterSize(16);
    instruction_text.setFillColor(sf::Color::Yellow);
    auto inst_bounds = instruction_text.getLocalBounds();
    instruction_text.setPosition(WINDOW_WIDTH / 2 - inst_bounds.width / 2, 420);

    error_text.setFont(g_font);
    error_text.setCharacterSize(16);
    error_text.setFillColor(sf::Color::Red);
    error_text.setPosition(200, 500);
}

void initialize_shop_ui()
{
    shop_panel.setSize({ 360.f, 220.f });
    shop_panel.setFillColor(sf::Color(0, 0, 0, 190));
    shop_panel.setOutlineColor(sf::Color(200, 200, 255, 200));
    shop_panel.setOutlineThickness(2.f);
    shop_panel.setPosition(WINDOW_WIDTH - 380.f, 40.f);

    shop_title_text.setFont(g_font);
    shop_title_text.setCharacterSize(22);
    shop_title_text.setFillColor(sf::Color::White);
    shop_title_text.setStyle(sf::Text::Bold);
    shop_title_text.setString("== SHOP ==");
    shop_title_text.setPosition(shop_panel.getPosition().x + 16.f, shop_panel.getPosition().y + 10.f);

    shop_desc_text.setFont(g_font);
    shop_desc_text.setCharacterSize(16);
    shop_desc_text.setFillColor(sf::Color(200, 200, 200));
    shop_desc_text.setString("All Items: 1 Gold");
    shop_desc_text.setPosition(shop_panel.getPosition().x + 16.f, shop_panel.getPosition().y + 46.f);

    gold_label_text.setFont(g_font);
    gold_label_text.setCharacterSize(16);
    gold_label_text.setFillColor(sf::Color::Yellow);
    gold_label_text.setPosition(shop_panel.getPosition().x + 16.f, shop_panel.getPosition().y + 72.f);

    hp_button.setSize({ 320.f, 48.f });
    hp_button.setFillColor(sf::Color(40, 80, 40, 220));
    hp_button.setOutlineColor(sf::Color(120, 200, 120));
    hp_button.setOutlineThickness(2.f);
    hp_button.setPosition(shop_panel.getPosition().x + 20.f, shop_panel.getPosition().y + 100.f);

    hp_label_text.setFont(g_font);
    hp_label_text.setCharacterSize(18);
    hp_label_text.setFillColor(sf::Color::White);
    hp_label_text.setString("Buy a HP potion");
    hp_label_text.setPosition(hp_button.getPosition().x + 12.f, hp_button.getPosition().y + 10.f);

    exp_button.setSize({ 320.f, 48.f });
    exp_button.setFillColor(sf::Color(40, 40, 80, 220));
    exp_button.setOutlineColor(sf::Color(120, 120, 200));
    exp_button.setOutlineThickness(2.f);
    exp_button.setPosition(shop_panel.getPosition().x + 20.f, shop_panel.getPosition().y + 160.f);

    exp_label_text.setFont(g_font);
    exp_label_text.setCharacterSize(18);
    exp_label_text.setFillColor(sf::Color::White);
    exp_label_text.setString("Buy a EXP potion");
    exp_label_text.setPosition(exp_button.getPosition().x + 12.f, exp_button.getPosition().y + 10.f);

    hp_btn_bounds = sf::FloatRect(hp_button.getPosition(), hp_button.getSize());
    exp_btn_bounds = sf::FloatRect(exp_button.getPosition(), exp_button.getSize());
}

void initialize_attendance_ui()
{
    attend_panel.setSize({ 360.f, 160.f });
    attend_panel.setFillColor(sf::Color(0, 0, 0, 190));
    attend_panel.setOutlineColor(sf::Color(255, 230, 150, 220));
    attend_panel.setOutlineThickness(2.f);
    attend_panel.setPosition(WINDOW_WIDTH - 380.f, 280.f);

    attend_title_text.setFont(g_font);
    attend_title_text.setCharacterSize(22);
    attend_title_text.setFillColor(sf::Color::White);
    attend_title_text.setStyle(sf::Text::Bold);
    attend_title_text.setString("== Attendance Book ==");
    attend_title_text.setPosition(attend_panel.getPosition().x + 16.f, attend_panel.getPosition().y + 10.f);

    attend_desc_text.setFont(g_font);
    attend_desc_text.setCharacterSize(16);
    attend_desc_text.setFillColor(sf::Color(230, 230, 230));
    attend_desc_text.setString("Once a day, Reward: 10 Gold");
    attend_desc_text.setPosition(attend_panel.getPosition().x + 16.f, attend_panel.getPosition().y + 46.f);

    attend_button.setSize({ 320.f, 48.f });
    attend_button.setFillColor(sf::Color(80, 60, 20, 220));
    attend_button.setOutlineColor(sf::Color(230, 200, 120));
    attend_button.setOutlineThickness(2.f);
    attend_button.setPosition(attend_panel.getPosition().x + 20.f, attend_panel.getPosition().y + 90.f);

    attend_btn_text.setFont(g_font);
    attend_btn_text.setCharacterSize(18);
    attend_btn_text.setFillColor(sf::Color::White);
    attend_btn_text.setString("Get Your Today's Reward");
    attend_btn_text.setPosition(attend_button.getPosition().x + 12.f, attend_button.getPosition().y + 10.f);

    attend_btn_bounds = sf::FloatRect(attend_button.getPosition(), attend_button.getSize());
}

void client_initialize()
{
    if (!self_texture.loadFromFile("player.png"))
    {
        std::cout << "PNG Loading Error!\n";
        std::exit(-1);
    }

    if (!other_texture.loadFromFile("other_player.png"))
    {
        std::cout << "PNG Loading Error!\n";
        std::exit(-1);
    }

    if (!randommonster_texture.loadFromFile("monster.png"))
    {
        std::cout << "PNG Loading Error!\n";
        std::exit(-1);
    }

    if (!followmonster_texture.loadFromFile("follow_monster.png"))
    {
        std::cout << "PNG Loading Error!\n";
        std::exit(-1);
    }

    if (!grass_texture.loadFromFile("grass_map.png"))
    {
        std::cout << "PNG Loading Error!\n";
        exit(-1);
    }

    grass_sprite.setTexture(grass_texture);

    if (!g_font.loadFromFile("malgun.ttf"))
    {
        std::cout << "Font Loading Error!\n";
        exit(-1);
    }

    if (!attack_texture.loadFromFile("slash.png"))
    {
        std::cout << "PNG Loading Error!\n";
        exit(-1);
    }

    if (!obstacle_texture.loadFromFile("obstacle.png"))
    {
        std::cout << "PNG Loading Error!\n";
        std::exit(-1);
    }

    grass_texture.setRepeated(true);
    grass_sprite.setTexture(grass_texture);

    obstacle_sprite.setTexture(obstacle_texture);
    float tex_x = (float)obstacle_texture.getSize().x;
    float tex_y = (float)obstacle_texture.getSize().y;
    float scale_x = TILE_WIDTH / tex_x;
    float scale_y = TILE_WIDTH / tex_y;
    obstacle_sprite.setScale(scale_x, scale_y);

    chat_input_text.setFont(g_font);
    chat_input_text.setCharacterSize(14);
    chat_input_text.setFillColor(sf::Color::Yellow);
    const float chat_box_height = CHAT_HISTORY_MAX * 20.0f;
    chat_input_text.setPosition(5.0f, WINDOW_HEIGHT - chat_box_height - 22.0f);

    initialize_login_screen();
    initialize_shop_ui();
    initialize_attendance_ui();
}

void client_finish()
{
    players.clear();
    delete board;
    delete pieces;
}

bool attempt_connection(const std::string& server_ip, const std::string& user_id)
{
    if (server_ip.empty()) 
    {
        error_message = "Server IP cannot be empty!";
        return false;
    }

    if (user_id.empty()) 
    {
        error_message = "User ID cannot be empty!";
        return false;
    }

    if (user_id.length() > 10) 
    {
        error_message = "User ID must be 10 characters or less!";
        return false;
    }

    sf::Socket::Status status = s_socket.connect(server_ip.c_str(), PORT_NUM);

    if (status != sf::Socket::Done) 
    {
        error_message = "Failed to connect to server!";
        return false;
    }

    s_socket.setBlocking(false);

    my_name = user_id;

    CS_LOGIN_PACKET login_pkt;
    login_pkt.size = sizeof(login_pkt);
    login_pkt.type = CS_LOGIN;
    memset(login_pkt.name, 0, NAME_SIZE);
    strncpy_s(login_pkt.name, user_id.c_str(), NAME_SIZE - 1);
    login_pkt.name[NAME_SIZE - 1] = '\0';

    unsigned char* p = reinterpret_cast<unsigned char*>(&login_pkt);
    size_t sent = 0;
    s_socket.send(&login_pkt, p[0], sent);

    avatar.set_name(login_pkt.name);

    return true;
}

void draw_login_screen()
{
    g_window->clear(sf::Color::Black);

    sf::RectangleShape background;
    background.setSize(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    background.setFillColor(sf::Color(20, 20, 40));
    g_window->draw(background);

    g_window->draw(login_title_text);
    g_window->draw(ip_label_text);
    g_window->draw(id_label_text);
    g_window->draw(ip_input_box);
    g_window->draw(id_input_box);
    g_window->draw(instruction_text);

    std::string ip_display = ip_string.toAnsiString();
    std::string id_display = id_string.toAnsiString();

    if (is_entering_ip) 
    {
        ip_display += "|";
    }

    else 
    {
        id_display += "|";
    }

    ip_input_text.setString(ip_display);
    id_input_text.setString(id_display);

    g_window->draw(ip_input_text);
    g_window->draw(id_input_text);

    if (!error_message.empty()) 
    {
        error_text.setString(error_message);
        g_window->draw(error_text);
    }
}

void handle_login_input(sf::Event& event)
{
    if (event.type == sf::Event::TextEntered) {
        auto uc = event.text.unicode;

        if (uc == '\b') 
        {
            if (is_entering_ip && !ip_string.isEmpty()) 
            {
                ip_string.erase(ip_string.getSize() - 1, 1);
            }

            else if (!is_entering_ip && !id_string.isEmpty()) 
            {
                id_string.erase(id_string.getSize() - 1, 1);
            }

            error_message.clear();
        }
        else if (uc >= 32 && uc < 127) 
        {
            if (is_entering_ip) 
            {
                ip_string += static_cast<char>(uc);
            }

            else 
            {
                if (id_string.getSize() < 10) 
                {
                    id_string += static_cast<char>(uc);
                }
            }
            error_message.clear();
        }
    }

    else if (event.type == sf::Event::KeyPressed) 
    {
        if (event.key.code == sf::Keyboard::Tab)
        {
            is_entering_ip = !is_entering_ip;

            if (is_entering_ip)
            {
                ip_input_box.setOutlineColor(sf::Color::Green);
                id_input_box.setOutlineColor(sf::Color::White);
            }

            else
            {
                ip_input_box.setOutlineColor(sf::Color::White);
                id_input_box.setOutlineColor(sf::Color::Green);
            }

            error_message.clear();
        }

        else if (event.key.code == sf::Keyboard::Enter) 
        {
            std::string server_ip = ip_string.toAnsiString();
            std::string user_id = id_string.toAnsiString();

            if (attempt_connection(server_ip, user_id)) 
            {
                current_state = GAME_RUNNING;
                error_message.clear();
            }
        }
    }
}

void draw_shop()
{
    if (!shop_open)
    {
        return;
    }

    std::ostringstream goldss;
    goldss << "Gold: " << avatar.m_gold;
    gold_label_text.setString(goldss.str());

    g_window->draw(shop_panel);
    g_window->draw(shop_title_text);
    g_window->draw(shop_desc_text);
    g_window->draw(gold_label_text);

    g_window->draw(hp_button);
    g_window->draw(exp_button);
    g_window->draw(hp_label_text);
    g_window->draw(exp_label_text);
}

void draw_attendance()
{
    if (!attendance_open)
    {
        return;
    }

    g_window->draw(attend_panel);
    g_window->draw(attend_title_text);
    g_window->draw(attend_desc_text);
    g_window->draw(attend_button);
    g_window->draw(attend_btn_text);
}

void ProcessPacket(char* ptr)
{
    switch (ptr[1])
    {
    case SC_LOGIN_FAIL:
    {
        std::cout << " LOGIN FAIL \n";
        error_message = "Login failed! Please try again.";
        current_state = LOGIN_SCREEN;
        s_socket.disconnect();
        break;
    }

    case SC_LOGIN_INFO:
    {
        SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
        g_myid = packet->id;

        avatar = OBJECT{ self_texture, 0, 0, (int)self_texture.getSize().x, (int)self_texture.getSize().y };
        avatar.id = g_myid;
        avatar.set_name(my_name.c_str());
        avatar.move(packet->x, packet->y);
        g_left_x = packet->x - SCREEN_WIDTH / 2;
        g_top_y = packet->y - SCREEN_HEIGHT / 2;
        avatar.set_stat(packet->hp, packet->max_hp, packet->exp, packet->level, packet->potion, packet->exp_potion, packet->gold);
        avatar.show();

        break;
    }

    case SC_ADD_OBJECT:
    {
        SC_ADD_OBJECT_PACKET* my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
        int id = my_packet->id;
        std::string nm(my_packet->name);

        if (id == g_myid)
        {
            avatar.id = id;
            avatar.move(my_packet->x, my_packet->y);
            avatar.show();
        }

        else if (id < MAX_USER)
        {
            players[id] = OBJECT{ other_texture, 0, 0, (int)other_texture.getSize().x, (int)other_texture.getSize().y };
            players[id].id = id;
            players[id].move(my_packet->x, my_packet->y);
            players[id].set_name(my_packet->name);
            players[id].show();
        }

        else
        {
            sf::Texture& tex = (nm.rfind("FOLLOW_", 0) == 0) ? followmonster_texture : randommonster_texture;
            players[id] = OBJECT{ tex, 0, 0, (int)tex.getSize().x, (int)tex.getSize().y };
            players[id].id = id;
            players[id].move(my_packet->x, my_packet->y);
            players[id].set_name(my_packet->name);
            players[id].set_stat(3, 3, 0, 1, 0, 0, 0);
            players[id].show();
        }

        break;
    }

    case SC_MOVE_OBJECT:
    {
        SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
        int other_id = my_packet->id;

        if (other_id == g_myid)
        {
            avatar.move(my_packet->x, my_packet->y);
            g_left_x = my_packet->x - SCREEN_WIDTH / 2;
            g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
        }

        else
        {
            players[other_id].move(my_packet->x, my_packet->y);
        }

        break;
    }

    case SC_REMOVE_OBJECT:
    {
        SC_REMOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
        int other_id = my_packet->id;

        if (other_id == g_myid)
        {
            avatar.hide();
        }

        else
        {
            players.erase(other_id);
        }

        break;
    }

    case SC_CHAT:
    {
        auto my_packet = reinterpret_cast<SC_CHAT_PACKET*>(ptr);
        unsigned char* raw = reinterpret_cast<unsigned char*>(ptr);
        std::string receivedName(my_packet->name, NAME_SIZE);

        std::cout << "[DEBUG-CLIENT] receivedName rawBuffer=[";

        for (int i = 0; i < NAME_SIZE; ++i)
        {
            if (my_packet->name[i] == '\0')
            {
                std::cout << "\\0 ";
            }

            else
            {
                std::cout << my_packet->name[i] << ' ';
            }
        }

        std::cout << "] �� str=\"" << receivedName << "\"\n";

        int other_id = my_packet->id;
        std::string senderName = std::string(my_packet->name);

        if (other_id == g_myid)
        {
            avatar.set_chat(my_packet->mess);
        }

        else
        {
            auto it = players.find(other_id);

            if (it != players.end())
            {
                it->second.set_chat(my_packet->mess);
            }
        }

        std::ostringstream oss;
        oss << senderName << ": " << my_packet->mess;
        chat_history.push_back(oss.str());

        if (chat_history.size() > CHAT_HISTORY_MAX)
        {
            chat_history.pop_front();
        }

        break;
    }

    case SC_STAT_CHANGE:
    {
        SC_STAT_CHANGEL_PACKET* stat_pkt = reinterpret_cast<SC_STAT_CHANGEL_PACKET*>(ptr);
        int target_id = stat_pkt->id;
        int new_hp = stat_pkt->hp;
        int new_maxhp = stat_pkt->max_hp;
        int new_exp = stat_pkt->exp;
        int new_level = stat_pkt->level;
        int new_potion = stat_pkt->potion;
        int new_exppotion = stat_pkt->exp_potion;
        int new_gold = stat_pkt->gold;

        if (target_id == g_myid)
        {
            avatar.set_stat(new_hp, new_maxhp, new_exp, new_level, new_potion, new_exppotion, new_gold);
        }

        else
        {
            auto it = players.find(target_id);

            if (it != players.end())
            {
                it->second.set_stat(new_hp, new_maxhp, new_exp, new_level, new_potion, new_exppotion, new_gold);
            }
        }

        break;
    }

    case SC_OBSTACLE:
    {
        auto obs = reinterpret_cast<SC_OBSTACLE_PACKET*>(ptr);
        int key = obs->y * W_WIDTH + obs->x;
        obstacle_set.insert(key);

        break;
    }

    case SC_REMOVE_OBSTACLE_SECTOR:
    {
        auto* pkt = reinterpret_cast<SC_REMOVE_OBSTACLE_SECTOR_PACKET*>(ptr);
        int sec = pkt->sector_idx;
        int sx = (sec % SECTOR_X) * SECTOR_SIZE;
        int sy = (sec / SECTOR_X) * SECTOR_SIZE;
        int ex = std::min(sx + SECTOR_SIZE, W_WIDTH);
        int ey = std::min(sy + SECTOR_SIZE, W_HEIGHT);

        for (int y = sy; y < ey; ++y)
        {
            int base = y * W_WIDTH;

            for (int x = sx; x < ex; ++x)
            {
                obstacle_set.erase(base + x);
            }
        }

        break;
    }

    default:
        printf(" UNKNOWN PACKET TYPE [%d] \n", ptr[1]);
    }
}

void process_data(char* net_buf, size_t io_byte)
{
    char* ptr = net_buf;
    static size_t in_packet_size = 0;
    static size_t saved_packet_size = 0;
    static char packet_buffer[BUF_SIZE];

    while (io_byte > 0)
    {
        if (in_packet_size == 0)
        {
            in_packet_size = ptr[0];
        }

        if (io_byte + saved_packet_size >= in_packet_size)
        {
            memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
            ProcessPacket(packet_buffer);
            ptr += in_packet_size - saved_packet_size;
            io_byte -= in_packet_size - saved_packet_size;
            in_packet_size = 0;
            saved_packet_size = 0;
        }

        else
        {
            memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
            saved_packet_size += io_byte;
            io_byte = 0;
        }
    }
}

void client_main()
{
    char net_buf[BUF_SIZE];
    size_t received;

    auto recv_result = s_socket.receive(net_buf, BUF_SIZE, received);

    if (recv_result == sf::Socket::Error)
    {
        std::wcout << L" Recv ERROR ! \n";
        exit(-1);
    }

    if (recv_result == sf::Socket::Disconnected)
    {
        std::wcout << L" DISCONNECTED \n";
        exit(-1);
    }

    if (recv_result != sf::Socket::NotReady && received > 0)
    {
        process_data(net_buf, received);
    }

    int px = g_left_x * TILE_WIDTH;
    int py = g_top_y * TILE_WIDTH;

    grass_sprite.setTextureRect(sf::IntRect(px, py, WINDOW_WIDTH, WINDOW_HEIGHT));
    grass_sprite.setPosition(0, 0);
    g_window->draw(grass_sprite);

    for (int i = 0; i < SCREEN_WIDTH; ++i)
    {
        for (int j = 0; j < SCREEN_HEIGHT; ++j)
        {
            int tx = i + g_left_x;
            int ty = j + g_top_y;

            if (tx < 0 || ty < 0 || tx >= W_WIDTH || ty >= W_HEIGHT)
            {
                continue;
            }

            int key = ty * W_WIDTH + tx;

            if (obstacle_set.count(key))
            {
                obstacle_sprite.setPosition(i * TILE_WIDTH, j * TILE_WIDTH);
                g_window->draw(obstacle_sprite);
            }
        }
    }

    avatar.draw();

    for (auto& pl : players)
    {
        pl.second.draw();
    }

    sf::Text text;
    text.setFont(g_font);
    char buf[128];
    sprintf_s(buf, "(%d, %d)  HP: %d  LEVEL: %d  EXP: %d", avatar.m_x, avatar.m_y, avatar.m_hp, avatar.m_level, avatar.m_exp);
    text.setString(buf);
    g_window->draw(text);

    const float chat_box_width = 300.0f;
    const float chat_box_height = static_cast<float>(CHAT_HISTORY_MAX * 20);

    sf::RectangleShape chat_background;
    chat_background.setSize(sf::Vector2f(chat_box_width, chat_box_height));
    chat_background.setFillColor(sf::Color(0, 0, 0, 150));
    chat_background.setPosition(0.0f, WINDOW_HEIGHT - chat_box_height);

    g_window->draw(chat_background);

    float line_height = 18.0f;
    float start_x = 5.0f;
    float start_y = WINDOW_HEIGHT - chat_box_height + 3.0f;

    sf::Text chat_linetext;
    chat_linetext.setFont(g_font);
    chat_linetext.setCharacterSize(14);
    chat_linetext.setFillColor(sf::Color::White);

    size_t idx = 0;

    for (const auto& line : chat_history)
    {
        sf::String ustr = sf::String::fromUtf8((const sf::Uint8*)line.c_str(), (const sf::Uint8*)(line.c_str() + line.size()));
        chat_linetext.setString(ustr);
        chat_linetext.setPosition(start_x, start_y + idx * line_height);
        g_window->draw(chat_linetext);
        ++idx;
    }

    sf::Time now = game_clock.getElapsedTime();
    std::vector<attack_effect> remaining;
    remaining.reserve(attack_effects.size());

    for (auto& fx : attack_effects)
    {
        if (now < fx.end_time)
        {
            g_window->draw(fx.sprite);
            remaining.push_back(fx);
        }
    }

    attack_effects.swap(remaining);
}

void draw_inventory()
{
    sf::RectangleShape bg;
    bg.setSize({ 300.f, 120.f });
    bg.setFillColor(sf::Color(0, 0, 0, 180));
    bg.setPosition(50.f, 50.f);
    g_window->draw(bg);

    sf::Text txt;
    txt.setFont(g_font);
    txt.setCharacterSize(18);
    txt.setFillColor(sf::Color::White);

    std::ostringstream ss;

    ss << "=== INVENTORY ===\n"
        << "Potion       : " << avatar.m_potion << "\n"
        << "Exp Potion   : " << avatar.m_exppotion << "\n"
        << "Gold         : " << avatar.m_gold;

    txt.setString(ss.str());
    txt.setPosition(60.f, 60.f);
    g_window->draw(txt);
}

void send_packet(void* packet)
{
    unsigned char* p = reinterpret_cast<unsigned char*>(packet);
    size_t sent = 0;
    s_socket.send(packet, p[0], sent);
}

void send_buy_item(unsigned char item_type)
{
    if (avatar.m_gold < 1)
    {
        chat_history.push_back(std::string("Gold is Not Enough."));

        if (chat_history.size() > CHAT_HISTORY_MAX)
        {
            chat_history.pop_front();
        }

        return;
    }

    CS_BUY_ITEM_PACKET pkt;
    pkt.size = sizeof(pkt);
    pkt.type = CS_BUY_ITEM;
    pkt.item_type = item_type;

    send_packet(&pkt);
}

void send_attendance_claim()
{
    CS_ATTEND_CLAIM_PACKET pkt;
    pkt.size = sizeof(pkt);
    pkt.type = CS_ATTEND_CLAIM;

    send_packet(&pkt);
}

void handle_game_input(sf::Event& event)
{
    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter && !is_typing)
    {
        is_typing = true;
        chat_string.clear();
        return;
    }

    if (is_typing)
    {
        if (event.type == sf::Event::TextEntered)
        {
            auto uc = event.text.unicode;

            if (uc == '\b')
            {
                if (!chat_string.isEmpty())
                {
                    chat_string.erase(chat_string.getSize() - 1, 1);
                }
            }
            else if (uc >= 32 && uc < 0x110000)
            {
                chat_string += uc;
            }
        }
        else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter)
        {
            if (!chat_string.isEmpty())
            {
                auto u8 = chat_string.toUtf8();
                std::string utf8(u8.begin(), u8.end());

                CS_CHAT_PACKET chat_pkt;
                chat_pkt.size = sizeof(chat_pkt);
                chat_pkt.type = CS_CHAT;
                memset(chat_pkt.mess, 0, CHAT_SIZE);
                strncpy_s(chat_pkt.mess, CHAT_SIZE, utf8.c_str(), _TRUNCATE);
                send_packet(&chat_pkt);
            }

            is_typing = false;
            chat_string.clear();
        }
        else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
        {
            is_typing = false;
            chat_string.clear();
        }
        return;
    }

    if (event.type == sf::Event::KeyPressed)
    {
        int direction = -1;

        switch (event.key.code)
        {
        case sf::Keyboard::Left:
        {
            direction = 2;

            break;
        }

        case sf::Keyboard::Right:
        {
            direction = 3;

            break;
        }

        case sf::Keyboard::Up:
        {
            direction = 0;

            break;
        }

        case sf::Keyboard::Down:
        {
            direction = 1;

            break;
        }

        case sf::Keyboard::Escape:
        {
            current_state = LOGIN_SCREEN;
            s_socket.disconnect();
            players.clear();
            chat_history.clear();
            obstacle_set.clear();
            attack_effects.clear();
            inventory_open = false;
            is_typing = false;
            error_message.clear();
            ip_string.clear();
            id_string.clear();
            is_entering_ip = true;
            ip_input_box.setOutlineColor(sf::Color::Green);
            id_input_box.setOutlineColor(sf::Color::White);

            if (s_socket.getRemotePort() != 0) 
            {
                s_socket.disconnect();
            }

            g_window->close();

            break;
        }

        case sf::Keyboard::I:
        {
            inventory_open = !inventory_open;

            break;
        }

        case sf::Keyboard::P:
        {
            shop_open = !shop_open;

            break;
        }

        case sf::Keyboard::O:
        {
            attendance_open = !attendance_open;

            if (attendance_open) 
            { 
                shop_open = false; 
                inventory_open = false; 
            }

            break;
        }

        case sf::Keyboard::Num1:
        {
            if (avatar.m_potion > 0)
            {
                CS_USE_POTION_PACKET use_pkt;
                use_pkt.size = sizeof(use_pkt);
                use_pkt.type = CS_USE_POTION;
                use_pkt.potion_type = 1;
                send_packet(&use_pkt);
            }

            break;
        }

        case sf::Keyboard::Num2:
        {
            if (avatar.m_exppotion > 0)
            {
                CS_USE_POTION_PACKET use_pkt;
                use_pkt.size = sizeof(use_pkt);
                use_pkt.type = CS_USE_POTION;
                use_pkt.potion_type = 2;
                send_packet(&use_pkt);
            }

            break;
        }

        case sf::Keyboard::Space:
        {
            CS_ATTACK_PACKET atk_pkt;
            atk_pkt.size = sizeof(atk_pkt);
            atk_pkt.type = CS_ATTACK;
            send_packet(&atk_pkt);

            sf::Time now = game_clock.getElapsedTime();

            for (int dx = -1; dx <= 1; ++dx)
            {
                for (int dy = -1; dy <= 1; ++dy)
                {
                    if (dx == 0 && dy == 0)
                    {
                        continue;
                    }

                    int tx = avatar.m_x + dx;
                    int ty = avatar.m_y + dy;

                    float sx = (tx - g_left_x) * TILE_WIDTH;
                    float sy = (ty - g_top_y) * TILE_WIDTH;

                    attack_effect fx;
                    fx.sprite.setTexture(attack_texture);
                    fx.sprite.setPosition(sx + TILE_WIDTH / 2 - attack_texture.getSize().x / 2, sy + TILE_WIDTH / 2 - attack_texture.getSize().y / 2);
                    fx.end_time = now + sf::seconds(0.3f);

                    attack_effects.push_back(fx);
                }
            }

            break;
        }
        }

        if (direction != -1)
        {
            CS_MOVE_PACKET move_pkt;
            move_pkt.size = sizeof(move_pkt);
            move_pkt.type = CS_MOVE;
            move_pkt.direction = direction;
            send_packet(&move_pkt);
        }
    }

    if (shop_open && event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left)
    {
        sf::Vector2f mousePos = g_window->mapPixelToCoords({ event.mouseButton.x, event.mouseButton.y });

        if (hp_btn_bounds.contains(mousePos))
        {
            send_buy_item(1);
        }

        else if (exp_btn_bounds.contains(mousePos))
        {
            send_buy_item(2);
        }
    }

    if (attendance_open && event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left)
    {
        sf::Vector2f mousePos = g_window->mapPixelToCoords({ event.mouseButton.x, event.mouseButton.y });

        if (attend_btn_bounds.contains(mousePos)) 
        {
            send_attendance_claim();
        }
    }
}

int main()
{
    std::wcout.imbue(std::locale("korean"));

    client_initialize();

    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "CLIENT");
    g_window = &window;

    while (window.isOpen())
    {
        sf::Event event;

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
                break;
            }

            if (current_state == LOGIN_SCREEN)
            {
                handle_login_input(event);
            }

            else if (current_state == GAME_RUNNING)
            {
                handle_game_input(event);
            }
        }

        if (current_state == LOGIN_SCREEN)
        {
            draw_login_screen();
        }

        else if (current_state == GAME_RUNNING)
        {
            window.clear();
            client_main();

            if (inventory_open)
            {
                draw_inventory();
            }

            draw_shop();

            draw_attendance();

            if (is_typing)
            {
                chat_input_text.setString(">" + chat_string + "|");
                window.draw(chat_input_text);
            }
        }

        window.display();
    }

    client_finish();
    return 0;
}