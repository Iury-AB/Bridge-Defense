#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stddef.h>
#include <json-c/json.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cerrno>

#define BUFFER_SIZE 2048

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec)
{
    os << "[ ";
    for (const auto &element : vec)
    {
        os << element << " ";
    }
    os << "]";
    return os;
}

void logexit(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

std::string constructJson(const std::vector<std::string> &attributeNames, const std::vector<std::string> &attributeValues)
{
    // Check if attributeNames and attributeValues have the same size
    if (attributeNames.size() != attributeValues.size())
    {
        return ""; // Return empty string if sizes don't match
    }

    std::stringstream ss;
    ss << "{"; // Start JSON object

    // Iterate through each attribute name-value pair
    for (size_t i = 0; i < attributeNames.size(); ++i)
    {
        // Add attribute name
        ss << "\"" << attributeNames[i] << "\": ";

        // Detect if the value is an array
        bool isArray = attributeValues[i].find('[') != std::string::npos && attributeValues[i].find(']') != std::string::npos;

        // Add attribute value
        // If the value is a string or an array, enclose it in double quotes
        if (isArray)
        {
            ss << attributeValues[i];
        }
        else if (attributeValues[i].find_first_not_of("0123456789") != std::string::npos)
        {
            ss << "\"" << attributeValues[i] << "\"";
        }
        else
        {
            ss << attributeValues[i];
        }

        // Add comma if not the last attribute
        if (i < attributeNames.size() - 1)
        {
            ss << ", ";
        }
    }

    ss << "}"; // End JSON object
    return ss.str();
}

/// @brief Constroi uma string Json e a envia para sockfd.
/// @param n Número de atributos (metado do tamanho do parâmetro atributos)
/// @param atributos  Deve forneces os nomes dos atributos primeiro, e depois seus valores.
/// @param sockfd socket para o qual o json será enviado.
/// @param _addr Endereço
/// @param _len Comprimento do endereço
void build_and_send(ssize_t n, std::vector<std::string> atributos, int sockfd, const sockaddr *_addr, socklen_t &_len)
{

    std::vector<std::string> types;
    std::vector<std::string> values;
    for (int i = 0; i < n; i++)
    {
        types.push_back(atributos[i]);
        values.push_back(atributos[n + i]);
    }
    std::string request = constructJson(types, values);

    ssize_t bytes_sent = sendto(sockfd, request.c_str(), request.length(), 0, _addr, _len);
    if (bytes_sent == -1)
    {
        logexit("sendto");
    }
}

class cannon
{
    int bridge;
    bool active;

public:
    std::vector<int> before;
    std::vector<int> after;

    cannon(int _bridge)
    {
        bridge = _bridge;
        active = false;
    }
    bool operator<(const cannon &other) const { return bridge < other.bridge; }
    void activate() { active = true; }
    friend std::ostream &operator<<(std::ostream &os, const cannon &_this);
    bool is_active() const { return active; }
};

std::ostream &operator<<(std::ostream &os, const cannon &_this)
{
    if (_this.is_active())
    {
        os << "X";
    }
    os << _this.before << ", " << _this.after << std::endl;
    return os;
}

class board
{
public:
    std::vector<std::vector<cannon>> bridges;
    int n_bridges;

    board(char buf[]);
    void place_ships(char buf[], int _river);
    friend std::ostream &operator<<(std::ostream &os, const board &_this);
    void clean_board();
};

std::ostream &operator<<(std::ostream &os, const board &_this)
{
    for (int i = 0; i <= 4; i++)
    {
        os << "\nRiver " << i << ":" << std::endl;
        os << _this.bridges[i];
    }
    return os;
}

board::board(char buf[])
{
    json_object *jsonData = json_tokener_parse(buf);
    json_object *cannons;
    if (!json_object_object_get_ex(jsonData, "cannons", &cannons))
    {
        logexit("Error: Unable to find 'cannons' array in JSON.\n");
    }

    n_bridges = 0;
    int arrayLength = json_object_array_length(cannons);
    for (int i = 0; i < arrayLength; i++)
    {
        // Get the current cannon element
        json_object *this_cannon = json_object_array_get_idx(cannons, i);

        // Get the first and second integers in the cannon
        int bridge, river;
        json_object *intObj;
        if ((intObj = json_object_array_get_idx(this_cannon, 0)))
            bridge = json_object_get_int(intObj);
        if ((intObj = json_object_array_get_idx(this_cannon, 1)))
            river = json_object_get_int(intObj);

        if (bridge > n_bridges)
            n_bridges = bridge;

        for (int j = 0; j <= 4; j++)
        {
            std::vector<cannon> new_bridge;
            bridges.push_back(new_bridge);
            for (int k = 0; k < n_bridges; k++)
            {
                if (k >= bridges[j].size())
                {
                    cannon new_cannon(k);
                    bridges[j].push_back(new_cannon);
                }

                if ((k == bridge - 1) && (j == river))
                {
                    bridges[j][k].activate();
                }
            }
        }
    }
}

void board::place_ships(char buf[], int _river)
{
    json_object *jsonData = json_tokener_parse(buf);
    json_object *ships;

    if (!json_object_object_get_ex(jsonData, "ships", &ships))
    {
        logexit("Error: Unable to find 'ships' array in JSON.\n");
    }

    json_object *intObj;
    int this_bridge;
    if ((!json_object_object_get_ex(jsonData, "bridge", &intObj)))
        logexit("Error: Unable to find 'bridge' array in JSON.\n");
    this_bridge = json_object_get_int(intObj);

    int arrayLength = json_object_array_length(ships);
    for (int i = 0; i < arrayLength; i++)
    {
        // Get the current ship element
        json_object *this_ship = json_object_array_get_idx(ships, i);
        // Get the ID of the ship

        int this_ID;
        if ((!json_object_object_get_ex(this_ship, "id", &intObj)))
            logexit("Error: Unable to find 'id' array in JSON.\n");
        this_ID = json_object_get_int(intObj);

        if (_river == 0)
        {
            bridges[0][this_bridge - 1].after.push_back(this_ID);
            bridges[1][this_bridge - 1].before.push_back(this_ID);
        }
        else if (_river == 3)
        {
            bridges[4][this_bridge - 1].before.push_back(this_ID);
            bridges[3][this_bridge - 1].after.push_back(this_ID);
        }
        else
        {
            bridges[_river][this_bridge - 1].after.push_back(this_ID);
            bridges[_river + 1][this_bridge - 1].before.push_back(this_ID);
        }
    }
}

void board::clean_board()
{
    for (int i = 0; i <= 4; i++)
    {
        for (int j = 0; j < n_bridges; j++)
        {
            bridges[i][j].before.clear();
            bridges[i][j].after.clear();
        }
    }
}

bool check_game_over(char buf[])
{

    json_object *jsonData = json_tokener_parse(buf);
    json_object *type;
    std::string message_type;

    if (!json_object_object_get_ex(jsonData, "type", &type))
    {
        logexit("Error: Unable to find 'type' array in JSON.\n");
    }
    message_type = json_object_get_string(type);
    if (message_type == "gameover")
    {
        std::cout << buf << std::endl;
        return true;
    }
    else
    {
        return false;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0] << " <hostname> <port1> <GAS>" << std::endl;
        return 1;
    }

    // parse arguments
    char host[100];
    strcpy(host, argv[1]);
    char port[10];
    strcpy(port, argv[2]);
    char gas[150];
    strcpy(gas, argv[3]);
    std::string ports[4];

    // Get server address info
    struct addrinfo hints, *res[4];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // UDP
    int sockets[4];
    for (int i = 0; i < 4; i++)
    {
        char i_port[5];
        sprintf(i_port, "%d", atoi(port) + i);
        ports[i] = i_port;
        if (getaddrinfo(host, ports[i].c_str(), &hints, &res[i]) != 0)
        {
            logexit("Error: Failed to get server address info\n");
            return 1;
        }

        // Create a socket
        sockets[i] = socket(res[i]->ai_family, res[i]->ai_socktype, res[i]->ai_protocol);
        if (sockets[i] == -1)
        {
            logexit("socket");
        }

        // Set timeout for recvfrom
        struct timeval timeout;
        timeout.tv_sec = 0; // 1 second
        timeout.tv_usec = 200000;
        if (setsockopt(sockets[i], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        {
            std::cerr << "Failed to set socket timeout: " << strerror(errno) << std::endl;
            close(sockets[i]);
            return 1;
        }
    }

    std::vector<std::string> auth_request = {"type", "auth", "authreq", gas};

    char buf[BUFFER_SIZE];
    memset(buf, 0, BUFFER_SIZE);

    for (int i = 0; i < 4; i++)
    {
        build_and_send(auth_request.size() / 2, auth_request, sockets[i], res[i]->ai_addr, res[i]->ai_addrlen);

        // Receive response from server
        socklen_t addr_len = res[i]->ai_addrlen;
        ssize_t bytes_received = recvfrom(sockets[i], buf, BUFFER_SIZE, 0, res[i]->ai_addr, &res[i]->ai_addrlen);
        if (bytes_received == -1)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                std::cerr << "Timeout occurred: No response received within the specified time. Resending message..." << std::endl;
                for (int r = 0; r < 10; r++)
                {
                    build_and_send(auth_request.size() / 2, auth_request, sockets[i], res[i]->ai_addr, res[i]->ai_addrlen);
                    memset(buf, 0, BUFFER_SIZE);
                    bytes_received = recvfrom(sockets[i], buf, BUFFER_SIZE, 0, res[i]->ai_addr, &res[i]->ai_addrlen);
                    if (bytes_received != -1)
                        break;
                }
            }
            else
            {
                std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
            }
        }
    }

    std::vector<std::string> cannon_request = {"type", "auth", "getcannons", gas};
    build_and_send(cannon_request.size() / 2, cannon_request, sockets[0], res[0]->ai_addr, res[0]->ai_addrlen);

    socklen_t addr_len = res[0]->ai_addrlen;
    memset(buf, 0, BUFFER_SIZE);
    ssize_t bytes_received = recvfrom(sockets[0], buf, BUFFER_SIZE, 0, res[0]->ai_addr, &addr_len);
    if (bytes_received == -1)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            std::cerr << "Timeout occurred: No response received within the specified time. Resending request..." << std::endl;
            for (int r = 0; r < 10; r++)
            {
                build_and_send(cannon_request.size() / 2, cannon_request, sockets[0], res[0]->ai_addr, res[0]->ai_addrlen);
                memset(buf, 0, BUFFER_SIZE);
                bytes_received = recvfrom(sockets[0], buf, BUFFER_SIZE, 0, res[0]->ai_addr, &res[0]->ai_addrlen);
                if (bytes_received != -1)
                    break;
            }
        }
        else
        {
            std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
        }
    }
    board this_board(buf);
    std::cout << buf << std::endl;

    int turn = 0;
    bool game_over = false;
    do
    {
        std::cout << "Turn [" << turn << "]==================" << std::endl;

        char alpha_turn[2];
        sprintf(alpha_turn, "%d", turn);
        for (int i = 0; i < 4; i++)
        {
            if (game_over)
            {
                break;
            }
            std::vector<std::string> turn_request = {"type", "auth", "turn", "getturn", gas, alpha_turn};
            build_and_send(turn_request.size() / 2, turn_request, sockets[i], res[i]->ai_addr, res[i]->ai_addrlen);

            for (int j = 0; j < this_board.n_bridges; j++)
            {
                memset(buf, 0, BUFFER_SIZE);
                bytes_received = recvfrom(sockets[i], buf, BUFFER_SIZE, 0, res[i]->ai_addr, &res[i]->ai_addrlen);
                if (bytes_received == -1)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                    {
                        std::cerr << "Timeout occurred: No response received within the specified time. Resending the message..." << std::endl;

                        build_and_send(turn_request.size() / 2, turn_request, sockets[i], res[i]->ai_addr, res[i]->ai_addrlen);
                        j = -1;
                        continue;
                    }
                    else
                    {
                        std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
                    }
                }
                game_over = check_game_over(buf);
                if (game_over)
                {
                    break;
                }
                this_board.place_ships(buf, i);
            }
        }
        if (game_over)
        {
            break;
        }

        for (int i = 0; i <= 4; i++)
        {
            for (int j = 0; j < this_board.n_bridges; j++)
            {
                if ((this_board.bridges[i][j].before.size() != 0) && (this_board.bridges[i][j].is_active()))
                {
                    std::stringstream ss;
                    ss << "[" << j + 1 << "," << i << "]";
                    std::string coordinates = ss.str();
                    std::stringstream sss;
                    sss << this_board.bridges[i][j].before[0];
                    std::string _id = sss.str();
                    std::vector<std::string> shot_message = {"type", "auth", "cannon", "id", "shot", gas, coordinates, _id};

                    build_and_send(shot_message.size() / 2, shot_message, sockets[i - 1], res[i - 1]->ai_addr, res[i - 1]->ai_addrlen);

                    memset(buf, 0, BUFFER_SIZE);
                    bytes_received = recvfrom(sockets[i - 1], buf, BUFFER_SIZE, 0, res[i - 1]->ai_addr, &res[i - 1]->ai_addrlen);
                    if (bytes_received == -1)
                    {
                        if (errno == EWOULDBLOCK || errno == EAGAIN)
                        {
                            std::cerr << "Timeout occurred: No response received within the specified time. Resending the message..." << std::endl;
                            for (int r = 0; r < 10; r++)
                            {
                                build_and_send(shot_message.size() / 2, shot_message, sockets[i - 1], res[i - 1]->ai_addr, res[i - 1]->ai_addrlen);
                                memset(buf, 0, BUFFER_SIZE);
                                bytes_received = recvfrom(sockets[i - 1], buf, BUFFER_SIZE, 0, res[i - 1]->ai_addr, &res[i - 1]->ai_addrlen);
                                if (bytes_received != -1)
                                    break;
                            }
                        }
                        else
                        {
                            std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
                        }
                    }
                    game_over = check_game_over(buf);
                    if (game_over)
                    {
                        break;
                    }
                }
                else if ((this_board.bridges[i][j].after.size() != 0) && (this_board.bridges[i][j].is_active()))
                {
                    std::stringstream ss;
                    ss << "[" << j + 1 << "," << i << "]";
                    std::string coordinates = ss.str();
                    std::stringstream sss;
                    sss << this_board.bridges[i][j].after[0];
                    std::string _id = sss.str();
                    std::vector<std::string> shot_message = {"type", "auth", "cannon", "id", "shot", gas, coordinates, _id};

                    build_and_send(shot_message.size() / 2, shot_message, sockets[i], res[i]->ai_addr, res[i]->ai_addrlen);

                    memset(buf, 0, BUFFER_SIZE);
                    bytes_received = recvfrom(sockets[i], buf, BUFFER_SIZE, 0, res[i]->ai_addr, &res[i]->ai_addrlen);
                    if (bytes_received == -1)
                    {
                        if (errno == EWOULDBLOCK || errno == EAGAIN)
                        {
                            std::cerr << "Timeout occurred: No response received within the specified time. Resending the message..." << std::endl;
                            for (int r = 0; r < 10; r++)
                            {
                                build_and_send(shot_message.size() / 2, shot_message, sockets[i], res[i]->ai_addr, res[i]->ai_addrlen);
                                memset(buf, 0, BUFFER_SIZE);
                                bytes_received = recvfrom(sockets[i], buf, BUFFER_SIZE, 0, res[i]->ai_addr, &res[i]->ai_addrlen);
                                if (bytes_received != -1)
                                    break;
                            }
                        }
                        else
                        {
                            std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
                        }
                    }
                    game_over = check_game_over(buf);
                    if (game_over)
                    {
                        break;
                    }
                }
            }
            if (game_over)
            {
                break;
            }
        }

        turn++;
        this_board.clean_board();
    } while (!game_over);

    std::vector<std::string> quit = {"type", "auth", "quit", gas};
    build_and_send(quit.size() / 2, quit, sockets[0], res[0]->ai_addr, res[0]->ai_addrlen);

    // Clean up
    for (int i = 0; i < 4; i++)
    {
        freeaddrinfo(res[i]);
        close(sockets[i]);
    }

    return 0;
}
