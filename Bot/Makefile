NAME = bot

SRCS = main.cpp Bot.cpp

CXX = c++

CXXFLAGS = -Wall -Wextra -Werror -g -std=c++98 -pedantic
SAN_FLAGS = -fsanitize=address,undefined,bounds
VAL_FLAGS = --leak-check=full --show-leak-kinds=all --track-fds=yes

OBJ_DIR = obj
DEPS_DIR = $(OBJ_DIR)/.deps

OBJS = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS:.cpp=.o)))
DEPS = $(addprefix $(DEPS_DIR)/, $(notdir $(SRCS:.cpp=.d)))

.PHONY: all
all: $(NAME)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(DEPS_DIR):
	@mkdir -p $(DEPS_DIR)

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR) $(DEPS_DIR)
	@printf "$(ITALIC)"
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(DEPS_DIR)/$(notdir $(<:.cpp=.d)) -c $< -o $@
	@printf "$(RESET)"

$(NAME): $(OBJS)
	@printf "$(ITALIC)"
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)
	@echo "$(GREEN)Executable is called: $(NAME)$(RESET)"

-include $(DEPS)

.PHONY: clean
clean:
	@printf "$(ITALIC)"
	rm -rf $(OBJ_DIR)
	@printf "$(RESET)"

.PHONY: fclean
fclean: clean
	@printf "$(ITALIC)"
	rm -rf $(NAME)
	@printf "$(RESET)"

.PHONY: re
re: fclean all

.PHONY: run
run: all
	@echo
	@./$(NAME) $(ARGS)

.PHONY: val
val: re
	@echo
	valgrind $(VAL_FLAGS) ./$(NAME) $(ARGS)

.PHONY: san
san: CXXFLAGS += $(SAN_FLAGS)
san: run

$(DEPS):
	@true

ITALIC=\033[3m
BOLD=\033[1m
RESET=\033[0m
GREEN=\033[32m
BLUE=\033[34m
