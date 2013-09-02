#include "ItemDisplay.h"

// All colors here must also be defined in TextColorMap
#define COLOR_REPLACEMENTS	\
	{"WHITE", "�c0"},		\
	{"RED", "�c1"},			\
	{"GREEN", "�c2"},		\
	{"BLUE", "�c3"},		\
	{"GOLD", "�c4"},		\
	{"GRAY", "�c5"},		\
	{"BLACK", "�c6"},		\
	{"TAN", "�c7"},			\
	{"ORANGE", "�c8"},		\
	{"YELLOW", "�c9"},		\
	{"PURPLE", "�c;"}

enum Operation {
	EQUAL,
	GREATER_THAN,
	LESS_THAN,
	NONE
};

std::map<std::string, ItemAttributes*> ItemAttributeMap;
vector<Rule*> RuleList;
vector<Rule*> MapRuleList;
vector<Rule*> IgnoreRuleList;
BYTE LastConditionType;

TrueCondition *trueCondition = new TrueCondition();
FalseCondition *falseCondition = new FalseCondition();

char* GemLevels[] = {
	"NONE",
	"Chipped",
	"Flawed",
	"Normal",
	"Flawless",
	"Perfect"
};

char* GemTypes[] = {
	"NONE",
	"Amethyst",
	"Diamond",
	"Emerald",
	"Ruby",
	"Sapphire",
	"Topaz",
	"Skull"
};

bool IsGem(BYTE nType) {
	return (nType >= 96 && nType <= 102);
}

BYTE GetGemLevel(char *itemCode) {
	BYTE c1 = itemCode[1];
	BYTE c2 = itemCode[2];
	BYTE gLevel = 1;
	if (c1 == 'f' || c2 == 'f') {
		gLevel = 2;
	} else if (c1 == 's' || c2 == 'u') {
		gLevel = 3;
	} else if (c1 == 'l' || c1 == 'z' || c2 == 'l') {
		gLevel = 4;
	} else if (c1 == 'p' || c2 == 'z') {
		gLevel = 5;
	}
	return gLevel;
}

char *GetGemLevelString(BYTE level) {
	return GemLevels[level];
}

BYTE GetGemType(BYTE nType) {
	return nType - 95;
}

char *GetGemTypeString(BYTE type) {
	return GemTypes[type];
}

bool IsRune(BYTE nType) {
	return (nType == 74);
}

void GetItemName(UnitItemInfo *uInfo, string &name) {
	for (vector<Rule*>::iterator it = RuleList.begin(); it != RuleList.end(); it++) {
		if ((*it)->Evaluate(uInfo, NULL)) {
			SubstituteNameVariables(uInfo->item, name, &(*it)->action);
			if ((*it)->action.stopProcessing) {
				break;
			}
		}
	}
}

void SubstituteNameVariables(UnitAny *item, string &name, Action *action) {
	char origName[128], sockets[4], code[4], ilvl[4], runename[16] = "", runenum[4] = "0";
	char gemtype[16] = "", gemlevel[16] = "";
	char *szCode = D2COMMON_GetItemText(item->dwTxtFileNo)->szCode;
	code[0] = szCode[0];
	code[1] = szCode[1];
	code[2] = szCode[2];
	code[3] = '\0';
	sprintf_s(sockets, "%d", D2COMMON_GetUnitStat(item, STAT_SOCKETS, 0));
	sprintf_s(ilvl, "%d", item->pItemData->dwItemLevel);
	sprintf_s(origName, "%s", name.c_str());
	BYTE nType = D2COMMON_GetItemText(item->dwTxtFileNo)->nType;
	if (IsRune(nType)) {
		sprintf_s(runenum, "%d", item->dwTxtFileNo - 609);
		sprintf_s(runename, name.substr(0, name.find(' ')).c_str());
	} else if (IsGem(nType)) {
		sprintf_s(gemlevel, "%s", GetGemLevelString(GetGemLevel(code)));
		sprintf_s(gemtype, "%s", GetGemTypeString(GetGemType(nType)));
	}
	ActionReplace replacements[] = {
		{"NAME", origName},
		{"SOCKETS", sockets},
		{"RUNENUM", runenum},
		{"RUNENAME", runename},
		{"GEMLEVEL", gemlevel},
		{"GEMTYPE", gemtype},
		{"ILVL", ilvl},
		{"CODE", code},
		COLOR_REPLACEMENTS
	};
	name.assign(action->name);
	for (int n = 0; n < sizeof(replacements) / sizeof(replacements[0]); n++) {
		if (name.find("%" + replacements[n].key + "%") == string::npos)
			continue;
		name.replace(name.find("%" + replacements[n].key + "%"), replacements[n].key.length() + 2, replacements[n].value);
	}
}

BYTE GetOperation(string *op) {
	if (op->length() < 1) {
		return NONE;
	} else if ((*op)[0] == '=') {
		return EQUAL;
	} else if ((*op)[0] == '<') {
		return LESS_THAN;
	} else if ((*op)[0] == '>') {
		return GREATER_THAN;
	}
	return NONE;
}

unsigned int GetItemCodeIndex(char codeChar) {
	// Characters '0'-'9' map to 0-9, and a-z map to 10-35
	return codeChar - (codeChar < 90 ? 48 : 87);
}

bool IntegerCompare(unsigned int Lvalue, BYTE operation, unsigned int Rvalue) {
	switch (operation) {
	case EQUAL:
		return Lvalue == Rvalue;
	case GREATER_THAN:
		return Lvalue > Rvalue;
	case LESS_THAN:
		return Lvalue < Rvalue;
	default:
		return false;
	}
}

void InitializeItemRules() {
	vector<pair<string, string>> rules = BH::config->ReadMapList("ItemDisplay");
	for (unsigned int i = 0; i < rules.size(); i++) {
		string buf;
		stringstream ss(rules[i].first);
		vector<string> tokens;
		while (ss >> buf) {
			tokens.push_back(buf);
		}

		LastConditionType = CT_None;
		Rule *r = new Rule();
		vector<Condition*> RawConditions;
		for (vector<string>::iterator tok = tokens.begin(); tok < tokens.end(); tok++) {
			Condition::BuildConditions(RawConditions, (*tok));
		}
		Condition::ProcessConditions(RawConditions, r->conditions);
		BuildAction(&(rules[i].second), &(r->action));

		RuleList.push_back(r);
		if (r->action.colorOnMap.length() > 0) {
			MapRuleList.push_back(r);
		} else if (r->action.name.length() == 0) {
			IgnoreRuleList.push_back(r);
		}
	}
}

void BuildAction(string *str, Action *act) {
	act->name = string(str->c_str());

	size_t map = act->name.find("%MAP%");
	if (map != string::npos) {
		string mapColor = "�c0";
		size_t lastColorPos = 0;
		ActionReplace colors[] = {
			COLOR_REPLACEMENTS
		};
		for (int n = 0; n < sizeof(colors) / sizeof(colors[0]); n++) {
			size_t pos = act->name.find("%" + colors[n].key + "%");
			if (pos != string::npos && pos < map && pos >= lastColorPos) {
				mapColor = colors[n].value;
				lastColorPos = pos;
			}
		}

		act->name.replace(map, 5, "");
		act->colorOnMap = mapColor;
	}
	size_t done = act->name.find("%CONTINUE%");
	if (done != string::npos) {
		act->name.replace(done, 10, "");
		act->stopProcessing = false;
	}
}

const string Condition::tokenDelims = "<=>";

// Implements the shunting-yard algorithm to evaluate condition expressions
// Returns a vector of conditions in Reverse Polish Notation
void Condition::ProcessConditions(vector<Condition*> &inputConditions, vector<Condition*> &processedConditions) {
	vector<Condition*> conditionStack;
	unsigned int size = inputConditions.size();
	for (unsigned int c = 0; c < size; c++) {
		Condition *input = inputConditions[c];
		if (input->conditionType == CT_Operand) {
			processedConditions.push_back(input);
		} else if (input->conditionType == CT_BinaryOperator || input->conditionType == CT_NegationOperator) {
			bool go = true;
			while (go) {
				if (conditionStack.size() > 0) {
					Condition *stack = conditionStack.back();
					if ((stack->conditionType == CT_NegationOperator || stack->conditionType == CT_BinaryOperator) &&
						input->conditionType == CT_BinaryOperator) {
						conditionStack.pop_back();
						processedConditions.push_back(stack);
					} else {
						go = false;
					}
				} else {
					go = false;
				}
			}
			conditionStack.push_back(input);
		} else if (input->conditionType == CT_LeftParen) {
			conditionStack.push_back(input);
		} else if (input->conditionType == CT_RightParen) {
			bool foundLeftParen = false;
			while (conditionStack.size() > 0 && !foundLeftParen) {
				Condition *stack = conditionStack.back();
				conditionStack.pop_back();
				if (stack->conditionType == CT_LeftParen) {
					foundLeftParen = true;
					break;
				} else {
					processedConditions.push_back(stack);
				}
			}
			if (!foundLeftParen) {
				// TODO: find a way to report error
				return;
			}
		}
	}
	while (conditionStack.size() > 0) {
		Condition *next = conditionStack.back();
		conditionStack.pop_back();
		if (next->conditionType == CT_LeftParen || next->conditionType == CT_RightParen) {
			// TODO: find a way to report error
			break;
		} else {
			processedConditions.push_back(next);
		}
	}
}

void Condition::BuildConditions(vector<Condition*> &conditions, string token) {
	size_t delPos = token.find_first_of(tokenDelims);
	string key;
	string delim = "";
	int value = 0;
	if (delPos != string::npos) {
		key = Trim(token.substr(0, delPos));
		delim = token.substr(delPos, 1);
		string valueStr = Trim(token.substr(delPos + 1));
		if (valueStr.length() > 0) {
			stringstream ss(valueStr);
			if ((ss >> value).fail()) {
				return;  // TODO: returning errors
			}
		}
	} else {
		key = token;
	}
	BYTE operation = GetOperation(&delim);

	int i;
	for (i = 0; i < (int)key.length(); i++) {
		if (key[i] == '!') {
			Condition::AddNonOperand(conditions, new NegationOperator());
		} else if (key[i] == '(') {
			Condition::AddNonOperand(conditions, new LeftParen());
		} else if (key[i] == ')') {
			Condition::AddNonOperand(conditions, new RightParen());
		} else {
			break;
		}
	}
	key.erase(0, i);

	unsigned int keylen = key.length();
	if (key.compare(0, 3, "AND") == 0) {
		Condition::AddNonOperand(conditions, new AndOperator());
	} else if (key.compare(0, 2, "OR") == 0) {
		Condition::AddNonOperand(conditions, new OrOperator());
	} else if (keylen >= 3 && !(isupper(key[0]) || isupper(key[1]) || isupper(key[2]))) {
		Condition::AddOperand(conditions, new ItemCodeCondition(key.substr(0, 3).c_str()));
	} else if (key.compare(0, 3, "ETH") == 0) {
		Condition::AddOperand(conditions, new FlagsCondition(ITEM_ETHEREAL));
	} else if (key.compare(0, 4, "SOCK") == 0) {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_SOCKETS, 0, operation, value));
	} else if (key.compare(0, 3, "SET") == 0) {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_SET));
	} else if (key.compare(0, 3, "MAG") == 0) {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_MAGIC));
	} else if (key.compare(0, 4, "RARE") == 0) {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_RARE));
	} else if (key.compare(0, 3, "UNI") == 0) {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_UNIQUE));
	} else if (key.compare(0, 2, "RW") == 0) {
		Condition::AddOperand(conditions, new FlagsCondition(ITEM_RUNEWORD));
	} else if (key.compare(0, 4, "NMAG") == 0) {
		Condition::AddOperand(conditions, new NonMagicalCondition());
	} else if (key.compare(0, 3, "SUP") == 0) {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_SUPERIOR));
	} else if (key.compare(0, 3, "INF") == 0) {
		Condition::AddOperand(conditions, new QualityCondition(ITEM_QUALITY_INFERIOR));
	} else if (key.compare(0, 4, "NORM") == 0) {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_NORMAL));
	} else if (key.compare(0, 3, "EXC") == 0) {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_EXCEPTIONAL));
	} else if (key.compare(0, 3, "ELT") == 0) {
		Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ELITE));
	} else if (key.compare(0, 2, "ID") == 0) {
		Condition::AddOperand(conditions, new FlagsCondition(ITEM_IDENTIFIED));
	} else if (key.compare(0, 4, "ILVL") == 0) {
		Condition::AddOperand(conditions, new ItemLevelCondition(operation, value));
	} else if (key.compare(0, 4, "RUNE") == 0) {
		Condition::AddOperand(conditions, new RuneCondition(operation, value));
	} else if (key.compare(0, 4, "GOLD") == 0) {
		Condition::AddOperand(conditions, new GoldCondition(operation, value));
	} else if (key.compare(0, 7, "GEMTYPE") == 0) {
		Condition::AddOperand(conditions, new GemTypeCondition(operation, value));
	} else if (key.compare(0, 3, "GEM") == 0) {
		Condition::AddOperand(conditions, new GemLevelCondition(operation, value));
	} else if (key.compare(0, 2, "ED") == 0) {
		Condition::AddOperand(conditions, new EDCondition(operation, value));
	} else if (key.compare(0, 3, "DEF") == 0) {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_DEFENSE, 0, operation, value));
	} else if (key.compare(0, 3, "RES") == 0) {
		Condition::AddOperand(conditions, new ResistAllCondition(operation, value));
	} else if (key.compare(0, 2, "EQ") == 0 && keylen >= 3) {
		if (key[2] == '1') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_HELM));
		} else if (key[2] == '2') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ARMOR));
		} else if (key[2] == '3') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SHIELD));
		} else if (key[2] == '4') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_GLOVES));
		} else if (key[2] == '5') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BOOTS));
		} else if (key[2] == '6') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BELT));
		} else if (key[2] == '7') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_CIRCLET));
		}
	} else if (key.compare(0, 2, "CL") == 0 && keylen >= 3) {
		if (key[2] == '1') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_DRUID_PELT));
		} else if (key[2] == '2') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BARBARIAN_HELM));
		} else if (key[2] == '3') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_PALADIN_SHIELD));
		} else if (key[2] == '4') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_NECROMANCER_SHIELD));
		} else if (key[2] == '5') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_ASSASSIN_KATAR));
		} else if (key[2] == '6') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SORCERESS_ORB));
		} else if (key[2] == '7') {
			Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_AMAZON_WEAPON));
		}
	} else if (key.compare(0, 2, "WP") == 0) {
		if (keylen >= 3) {
			if (key[2] == '1') {
				if (keylen >= 4 && key[3] == '0') {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_CROSSBOW));
				} else if (keylen >= 4 && key[3] == '1') {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_STAFF));
				} else if (keylen >= 4 && key[3] == '2') {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_WAND));
				} else if (keylen >= 4 && key[3] == '3') {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SCEPTER));
				} else {
					Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_AXE));
				}
			} else if (key[2] == '2') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_MACE));
			} else if (key[2] == '3') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SWORD));
			} else if (key[2] == '4') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_DAGGER));
			} else if (key[2] == '5') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_THROWING));
			} else if (key[2] == '6') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_JAVELIN));
			} else if (key[2] == '7') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_SPEAR));
			} else if (key[2] == '8') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_POLEARM));
			} else if (key[2] == '9') {
				Condition::AddOperand(conditions, new ItemGroupCondition(ITEM_GROUP_BOW));
			}
		}
	} else if (key.compare(0, 2, "SK") == 0) {
		int num = -1;
		stringstream ss(key.substr(2));
		if ((ss >> num).fail() || num < 0 || num > STAT_MAX) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_SINGLESKILL, num, operation, value));
	} else if (key.compare(0, 4, "CLSK") == 0) {
		int num = -1;
		stringstream ss(key.substr(4));
		if ((ss >> num).fail() || num < 0 || num >= CLASS_NA) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_CLASSSKILLS, num, operation, value));
	} else if (key.compare(0, 5, "ALLSK") == 0) {
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_ALLSKILLS, 0, operation, value));
	} else if (key.compare(0, 5, "TABSK") == 0) {
		int num = -1;
		stringstream ss(key.substr(5));
		if ((ss >> num).fail() || num < 0 || num >= SKILLTAB_MAX) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(STAT_SKILLTAB, num, operation, value));
	} else if (key.compare(0, 4, "STAT") == 0) {
		int num = -1;
		stringstream ss(key.substr(4));
		if ((ss >> num).fail() || num < 0 || num > STAT_MAX) {
			return;
		}
		Condition::AddOperand(conditions, new ItemStatCondition(num, 0, operation, value));
	}

	for (i = token.length() - 1; i >= 0; i--) {
		if (token[i] == '!') {
			Condition::AddNonOperand(conditions, new NegationOperator());
		} else if (token[i] == '(') {
			Condition::AddNonOperand(conditions, new LeftParen());
		} else if (token[i] == ')') {
			Condition::AddNonOperand(conditions, new RightParen());
		} else {
			break;
		}
	}
}

// Insert extra AND operators to stay backwards compatible with old config
// that implicitly ANDed all conditions
void Condition::AddOperand(vector<Condition*> &conditions, Condition *cond) {
	if (LastConditionType == CT_Operand || LastConditionType == CT_RightParen) {
		conditions.push_back(new AndOperator());
	}
	conditions.push_back(cond);
	LastConditionType = CT_Operand;
}

void Condition::AddNonOperand(vector<Condition*> &conditions, Condition *cond) {
	if ((cond->conditionType == CT_NegationOperator || cond->conditionType == CT_LeftParen) &&
		(LastConditionType == CT_Operand || LastConditionType == CT_RightParen)) {
		conditions.push_back(new AndOperator());
	}
	conditions.push_back(cond);
	LastConditionType = cond->conditionType;
}

bool Condition::Evaluate(UnitItemInfo *uInfo, ItemInfo *info, Condition *arg1, Condition *arg2) {
	// Arguments will vary based on where we're called from.
	// We will have either *info set (if called on reception of packet 0c9c, in which case
	// the normal item structures won't have been set up yet), or *uInfo otherwise.
	if (info) {
		return EvaluateInternalFromPacket(info, arg1, arg2);
	}
	return EvaluateInternal(uInfo, arg1, arg2);
}

bool TrueCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return true;
}
bool TrueCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return true;
}

bool FalseCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return false;
}
bool FalseCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return false;
}

bool NegationOperator::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return !arg1->Evaluate(uInfo, NULL, arg1, arg2);
}
bool NegationOperator::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return !arg1->Evaluate(NULL, info, arg1, arg2);
}

bool LeftParen::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return false;
}
bool LeftParen::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return false;
}

bool RightParen::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return false;
}
bool RightParen::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return false;
}

bool AndOperator::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return arg1->Evaluate(uInfo, NULL, NULL, NULL) && arg2->Evaluate(uInfo, NULL, NULL, NULL);
}
bool AndOperator::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return arg1->Evaluate(NULL, info, NULL, NULL) && arg2->Evaluate(NULL, info, NULL, NULL);
}

bool OrOperator::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return arg1->Evaluate(uInfo, NULL, NULL, NULL) || arg2->Evaluate(uInfo, NULL, NULL, NULL);
}
bool OrOperator::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return arg1->Evaluate(NULL, info, NULL, NULL) || arg2->Evaluate(NULL, info, NULL, NULL);
}

bool ItemCodeCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return (targetCode[0] == uInfo->itemCode[0] && targetCode[1] == uInfo->itemCode[1] && targetCode[2] == uInfo->itemCode[2]);
}
bool ItemCodeCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return (targetCode[0] == info->code[0] && targetCode[1] == info->code[1] && targetCode[2] == info->code[2]);
}

bool FlagsCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return ((uInfo->item->pItemData->dwFlags & flag) > 0);
}
bool FlagsCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	switch (flag) {
	case ITEM_ETHEREAL:
		return info->ethereal;
	case ITEM_IDENTIFIED:
		return info->identified;
	case ITEM_RUNEWORD:
		return info->runeword;
	}
	return false;
}

bool QualityCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return (uInfo->item->pItemData->dwQuality == quality);
}
bool QualityCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return (info->quality == quality);
}

bool NonMagicalCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return (uInfo->item->pItemData->dwQuality == ITEM_QUALITY_INFERIOR ||
			uInfo->item->pItemData->dwQuality == ITEM_QUALITY_NORMAL ||
			uInfo->item->pItemData->dwQuality == ITEM_QUALITY_SUPERIOR);
}
bool NonMagicalCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return (info->quality == ITEM_QUALITY_INFERIOR ||
			info->quality == ITEM_QUALITY_NORMAL ||
			info->quality == ITEM_QUALITY_SUPERIOR);
}

bool GemLevelCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	BYTE nType = D2COMMON_GetItemText(uInfo->item->dwTxtFileNo)->nType;
	if (IsGem(nType)) {
		return IntegerCompare(GetGemLevel(uInfo->itemCode), operation, gemLevel);
	}
	return false;
}
bool GemLevelCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	if (info->attrs->category.compare(0, 3, "Gem") == 0) {
		return IntegerCompare(GetGemLevel(info->code), operation, gemLevel);
	}
	return false;
}

bool GemTypeCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	BYTE nType = D2COMMON_GetItemText(uInfo->item->dwTxtFileNo)->nType;
	if (IsGem(nType)) {
		return IntegerCompare(GetGemType(nType), operation, gemType);
	}
	return false;
}
bool GemTypeCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	if (info->attrs->category.compare(0, 3, "Gem") == 0) {
		for (BYTE nType = 0; nType < sizeof(GemTypes); nType++) {
			if (info->attrs->name.find(GemTypes[nType]) != string::npos) {
				return IntegerCompare(nType, operation, gemType);
			}
		}
	}
	return false;
}

bool RuneCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	if (IsRune(D2COMMON_GetItemText(uInfo->item->dwTxtFileNo)->nType)) {
		return IntegerCompare(uInfo->item->dwTxtFileNo - 609, operation, runeNumber);
	}
	return false;
}
bool RuneCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	if (info->code[0] == 'r' &&
		info->code[1] >= '0' && info->code[1] <= '9' &&
		info->code[2] >= '0' && info->code[2] <= '9') {
		return IntegerCompare(((info->code[1] - '0') * 10) + info->code[2] - '0', operation, runeNumber);
	}
	return false;
}

bool GoldCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return false; // can only evaluate this from packet data
}
bool GoldCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	if (info->code[0] == 'g' && info->code[1] == 'l' && info->code[2] == 'd') {
		return IntegerCompare(info->amount, operation, goldAmount);
	}
	return false;
}

bool ItemLevelCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return IntegerCompare(uInfo->item->pItemData->dwItemLevel, operation, itemLevel);
}
bool ItemLevelCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return IntegerCompare(info->level, operation, itemLevel);
}

bool ItemGroupCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return ((uInfo->attrs->flags & itemGroup) > 0);
}
bool ItemGroupCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	return ((info->attrs->flags & itemGroup) > 0);
}

bool EDCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	// Either enhanced defense or enhanced damage depending on item type
	WORD stat;
	if (uInfo->attrs->flags & ITEM_GROUP_ALLARMOR) {
		stat = STAT_ENHANCEDDEFENSE;
	} else {
		// Normal %ED will have the same value for STAT_ENHANCEDMAXIMUMDAMAGE and STAT_ENHANCEDMINIMUMDAMAGE
		stat = STAT_ENHANCEDMAXIMUMDAMAGE;
	}

	// Pulled from JSUnit.cpp in d2bs
	DWORD value = 0;
	Stat aStatList[256] = { NULL };
	StatList* pStatList = D2COMMON_GetStatList(uInfo->item, NULL, 0x40);
	if (pStatList) {
		DWORD dwStats = D2COMMON_CopyStatList(pStatList, (Stat*)aStatList, 256);
		for (UINT i = 0; i < dwStats; i++) {
			if (aStatList[i].wStatIndex == stat && aStatList[i].wSubIndex == 0) {
				value += aStatList[i].dwStatValue;
			}
		}
	}
	return IntegerCompare(value, operation, targetED);
}
bool EDCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	// Either enhanced defense or enhanced damage depending on item type
	WORD stat;
	if (info->attrs->flags & ITEM_GROUP_ALLARMOR) {
		stat = STAT_ENHANCEDDEFENSE;
	} else {
		// Normal %ED will have the same value for STAT_ENHANCEDMAXIMUMDAMAGE and STAT_ENHANCEDMINIMUMDAMAGE
		stat = STAT_ENHANCEDMAXIMUMDAMAGE;
	}

	DWORD value = 0;
	for (vector<ItemProperty>::iterator prop = info->properties.begin(); prop < info->properties.end(); prop++) {
		if (prop->stat == stat) {
			value += prop->value;
		}
	}
	return IntegerCompare(value, operation, targetED);
}

bool ItemStatCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	return IntegerCompare(D2COMMON_GetUnitStat(uInfo->item, itemStat, itemStat2), operation, targetStat);
}
bool ItemStatCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	int num = 0;
	switch (itemStat) {
	case STAT_SOCKETS:
		return IntegerCompare(info->sockets, operation, targetStat);
	case STAT_DEFENSE:
		return IntegerCompare(GetDefense(info), operation, targetStat);
	case STAT_SINGLESKILL:
		for (vector<ItemProperty>::iterator prop = info->properties.begin(); prop < info->properties.end(); prop++) {
			if (prop->stat == STAT_SINGLESKILL && prop->skill == itemStat2) {
				num += prop->value;
			}
		}
		return IntegerCompare(num, operation, targetStat);
	case STAT_CLASSSKILLS:
		for (vector<ItemProperty>::iterator prop = info->properties.begin(); prop < info->properties.end(); prop++) {
			if (prop->stat == STAT_CLASSSKILLS && prop->characterClass == itemStat2) {
				num += prop->value;
			}
		}
		return IntegerCompare(num, operation, targetStat);
	case STAT_SKILLTAB:
		for (vector<ItemProperty>::iterator prop = info->properties.begin(); prop < info->properties.end(); prop++) {
			if (prop->stat == STAT_SKILLTAB && (prop->characterClass * 8 + prop->tab) == itemStat2) {
				num += prop->value;
			}
		}
		return IntegerCompare(num, operation, targetStat);
	case STAT_ALLSKILLS:
		for (vector<ItemProperty>::iterator prop = info->properties.begin(); prop < info->properties.end(); prop++) {
			if (prop->stat == STAT_ALLSKILLS) {
				num += prop->value;
			}
		}
		return IntegerCompare(num, operation, targetStat);
	}
	return false;
}

bool ResistAllCondition::EvaluateInternal(UnitItemInfo *uInfo, Condition *arg1, Condition *arg2) {
	int fRes = D2COMMON_GetUnitStat(uInfo->item, STAT_FIRERESIST, 0);
	int lRes = D2COMMON_GetUnitStat(uInfo->item, STAT_LIGHTNINGRESIST, 0);
	int cRes = D2COMMON_GetUnitStat(uInfo->item, STAT_COLDRESIST, 0);
	int pRes = D2COMMON_GetUnitStat(uInfo->item, STAT_POISONRESIST, 0);
	return (IntegerCompare(fRes, operation, targetStat) &&
			IntegerCompare(lRes, operation, targetStat) &&
			IntegerCompare(cRes, operation, targetStat) &&
			IntegerCompare(pRes, operation, targetStat));
}
bool ResistAllCondition::EvaluateInternalFromPacket(ItemInfo *info, Condition *arg1, Condition *arg2) {
	int fRes = 0, lRes = 0, cRes = 0, pRes = 0;
	for (vector<ItemProperty>::iterator prop = info->properties.begin(); prop < info->properties.end(); prop++) {
		if (prop->stat == STAT_FIRERESIST) {
			fRes += prop->value;
		} else if (prop->stat == STAT_LIGHTNINGRESIST) {
			lRes += prop->value;
		} else if (prop->stat == STAT_COLDRESIST) {
			cRes += prop->value;
		} else if (prop->stat == STAT_POISONRESIST) {
			pRes += prop->value;
		}
	}
	return (IntegerCompare(fRes, operation, targetStat) &&
			IntegerCompare(lRes, operation, targetStat) &&
			IntegerCompare(cRes, operation, targetStat) &&
			IntegerCompare(pRes, operation, targetStat));
}


ItemAttributes ItemAttributeList[] = {
	{"Cap", "cap", "Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Skull Cap", "skp", "Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Helm", "hlm", "Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Full Helm", "fhl", "Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Great Helm", "ghm", "Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Crown", "crn", "Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Mask", "msk", "Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Quilted Armor", "qui", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Leather Armor", "lea", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Hard Leather Armor", "hla", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Studded Leather", "stu", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Ring Mail", "rng", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Scale Mail", "scl", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Chain Mail", "chn", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Breast Plate", "brs", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Splint Mail", "spl", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Plate Mail", "plt", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Field Plate", "fld", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Gothic Plate", "gth", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Full Plate Mail", "ful", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Ancient Armor", "aar", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Light Plate", "ltp", "Armor", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Buckler", "buc", "Shield", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Small Shield", "sml", "Shield", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Large Shield", "lrg", "Shield", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Kite Shield", "kit", "Shield", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Tower Shield", "tow", "Shield", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Gothic Shield", "gts", "Shield", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Leather Gloves", "lgl", "Gloves", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Heavy Gloves", "vgl", "Gloves", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Chain Gloves", "mgl", "Gloves", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Light Gauntlets", "tgl", "Gloves", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Gauntlets", "hgl", "Gloves", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Boots", "lbt", "Boots", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Heavy Boots", "vbt", "Boots", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Chain Boots", "mbt", "Boots", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Light Plated Boots", "tbt", "Boots", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Greaves", "hbt", "Boots", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Sash", "lbl", "Belt", 2, 1, 0, 0, 0, 1, 0, 0},
	{"Light Belt", "vbl", "Belt", 2, 1, 0, 0, 0, 1, 0, 0},
	{"Belt", "mbl", "Belt", 2, 1, 0, 0, 0, 1, 0, 0},
	{"Heavy Belt", "tbl", "Belt", 2, 1, 0, 0, 0, 1, 0, 0},
	{"Plated Belt", "hbl", "Belt", 2, 1, 0, 0, 0, 1, 0, 0},
	{"Bone Helm", "bhm", "Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Bone Shield", "bsh", "Shield", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Spiked Shield", "spk", "Shield", 2, 3, 0, 0, 0, 1, 0, 0},
	{"War Hat", "xap", "Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Sallet", "xkp", "Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Casque", "xlm", "Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Basinet", "xhl", "Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Winged Helm", "xhm", "Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Grand Crown", "xrn", "Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Death Mask", "xsk", "Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Ghost Armor", "xui", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Serpentskin Armor", "xea", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Demonhide Armor", "xla", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Trellised Armor", "xtu", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Linked Mail", "xng", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Tigulated Mail", "xcl", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Mesh Armor", "xhn", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Cuirass", "xrs", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Russet Armor", "xpl", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Templar Coat", "xlt", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Sharktooth Armor", "xld", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Embossed Plate", "xth", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Chaos Armor", "xul", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Ornate Plate", "xar", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Mage Plate", "xtp", "Armor", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Defender", "xuc", "Shield", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Round Shield", "xml", "Shield", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Scutum", "xrg", "Shield", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Dragon Shield", "xit", "Shield", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Pavise", "xow", "Shield", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Ancient Shield", "xts", "Shield", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Demonhide Gloves", "xlg", "Gloves", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Sharkskin Gloves", "xvg", "Gloves", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Heavy Bracers", "xmg", "Gloves", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Battle Gauntlets", "xtg", "Gloves", 2, 2, 0, 0, 0, 2, 0, 0},
	{"War Gauntlets", "xhg", "Gloves", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Demonhide Boots", "xlb", "Boots", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Sharkskin Boots", "xvb", "Boots", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Mesh Boots", "xmb", "Boots", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Battle Boots", "xtb", "Boots", 2, 2, 0, 0, 0, 2, 0, 0},
	{"War Boots", "xhb", "Boots", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Demonhide Sash", "zlb", "Belt", 2, 1, 0, 0, 0, 2, 0, 0},
	{"Sharkskin Belt", "zvb", "Belt", 2, 1, 0, 0, 0, 2, 0, 0},
	{"Mesh Belt", "zmb", "Belt", 2, 1, 0, 0, 0, 2, 0, 0},
	{"Battle Belt", "ztb", "Belt", 2, 1, 0, 0, 0, 2, 0, 0},
	{"War Belt", "zhb", "Belt", 2, 1, 0, 0, 0, 2, 0, 0},
	{"Grim Helm", "xh9", "Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Grim Shield", "xsh", "Shield", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Barbed Shield", "xpk", "Shield", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Wolf Head", "dr1", "Druid Pelt", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Hawk Helm", "dr2", "Druid Pelt", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Antlers", "dr3", "Druid Pelt", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Falcon Mask", "dr4", "Druid Pelt", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Spirit Mask", "dr5", "Druid Pelt", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Jawbone Cap", "ba1", "Barbarian Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Fanged Helm", "ba2", "Barbarian Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Horned Helm", "ba3", "Barbarian Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Assault Helmet", "ba4", "Barbarian Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Avenger Guard", "ba5", "Barbarian Helm", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Targe", "pa1", "Paladin Shield", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Rondache", "pa2", "Paladin Shield", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Heraldic Shield", "pa3", "Paladin Shield", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Aerin Shield", "pa4", "Paladin Shield", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Crown Shield", "pa5", "Paladin Shield", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Preserved Head", "ne1", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Zombie Head", "ne2", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Unraveller Head", "ne3", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Gargoyle Head", "ne4", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Demon Head", "ne5", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Circlet", "ci0", "Circlet", 2, 2, 0, 0, 0, 1, 0, 0},
	{"Coronet", "ci1", "Circlet", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Tiara", "ci2", "Circlet", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Diadem", "ci3", "Circlet", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Shako", "uap", "Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Hydraskull", "ukp", "Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Armet", "ulm", "Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Giant Conch", "uhl", "Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Spired Helm", "uhm", "Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Corona", "urn", "Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Demonhead", "usk", "Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Dusk Shroud", "uui", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Wyrmhide", "uea", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Scarab Husk", "ula", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Wire Fleece", "utu", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Diamond Mail", "ung", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Loricated Mail", "ucl", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Boneweave", "uhn", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Great Hauberk", "urs", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Balrog Skin", "upl", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Hellforge Plate", "ult", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Kraken Shell", "uld", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Lacquered Plate", "uth", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Shadow Plate", "uul", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Sacred Armor", "uar", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Archon Plate", "utp", "Armor", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Heater", "uuc", "Shield", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Luna", "uml", "Shield", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Hyperion", "urg", "Shield", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Monarch", "uit", "Shield", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Aegis", "uow", "Shield", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Ward", "uts", "Shield", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Bramble Mitts", "ulg", "Gloves", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Vampirebone Gloves", "uvg", "Gloves", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Vambraces", "umg", "Gloves", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Crusader Gauntlets", "utg", "Gloves", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Ogre Gauntlets", "uhg", "Gloves", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Wyrmhide Boots", "ulb", "Boots", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Scarabshell Boots", "uvb", "Boots", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Boneweave Boots", "umb", "Boots", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Mirrored Boots", "utb", "Boots", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Myrmidon Greaves", "uhb", "Boots", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Spiderweb Sash", "ulc", "Belt", 2, 1, 0, 0, 0, 3, 0, 0},
	{"Vampirefang Belt", "uvc", "Belt", 2, 1, 0, 0, 0, 3, 0, 0},
	{"Mithril Coil", "umc", "Belt", 2, 1, 0, 0, 0, 3, 0, 0},
	{"Troll Belt", "utc", "Belt", 2, 1, 0, 0, 0, 3, 0, 0},
	{"Colossus Girdle", "uhc", "Belt", 2, 1, 0, 0, 0, 3, 0, 0},
	{"Bone Visage", "uh9", "Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Troll Nest", "ush", "Shield", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Blade Barrier", "upk", "Shield", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Alpha Helm", "dr6", "Druid Pelt", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Griffon Headdress", "dr7", "Druid Pelt", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Hunter's Guise", "dr8", "Druid Pelt", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Sacred Feathers", "dr9", "Druid Pelt", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Totemic Mask", "dra", "Druid Pelt", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Jawbone Visor", "ba6", "Barbarian Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Lion Helm", "ba7", "Barbarian Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Rage Mask", "ba8", "Barbarian Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Savage Helmet", "ba9", "Barbarian Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Slayer Guard", "baa", "Barbarian Helm", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Akaran Targe", "pa6", "Paladin Shield", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Akaran Rondache", "pa7", "Paladin Shield", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Protector Shield", "pa8", "Paladin Shield", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Gilded Shield", "pa9", "Paladin Shield", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Royal Shield", "paa", "Paladin Shield", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Mummified Trophy", "ne6", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Fetish Trophy", "ne7", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Sexton Trophy", "ne8", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Cantor Trophy", "ne9", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Hierophant Trophy", "nea", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 2, 0, 0},
	{"Blood Spirit", "drb", "Druid Pelt", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Sun Spirit", "drc", "Druid Pelt", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Earth Spirit", "drd", "Druid Pelt", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Sky Spirit", "dre", "Druid Pelt", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Dream Spirit", "drf", "Druid Pelt", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Carnage Helm", "bab", "Barbarian Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Fury Visor", "bac", "Barbarian Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Destroyer Helm", "bad", "Barbarian Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Conqueror Crown", "bae", "Barbarian Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Guardian Crown", "baf", "Barbarian Helm", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Sacred Targe", "pab", "Paladin Shield", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Sacred Rondache", "pac", "Paladin Shield", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Kurast Shield", "pad", "Paladin Shield", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Zakarum Shield", "pae", "Paladin Shield", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Vortex Shield", "paf", "Paladin Shield", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Minion Skull", "neb", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Hellspawn Skull", "neg", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Overseer Skull", "ned", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Succubus Skull", "nee", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Bloodlord Skull", "nef", "Necromancer Shrunken Head", 2, 2, 0, 0, 0, 3, 0, 0},
	{"Elixir", "elx", "Elixir", 1, 1, 0, 1, 0, 0, 0, 0},
	{"hpo", "hpo", "Health Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"mpo", "mpo", "Mana Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"hpf", "hpf", "Health Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"mpf", "mpf", "Mana Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Stamina Potion", "vps", "Stamina Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Antidote Potion", "yps", "Antidote Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Rejuvenation Potion", "rvs", "Rejuvenation Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Full Rejuvenation Potion", "rvl", "Rejuvenation Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Thawing Potion", "wms", "Thawing Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Tome of Town Portal", "tbk", "Tome", 1, 2, 1, 1, 0, 0, 0, 0},
	{"Tome of Identify", "ibk", "Tome", 1, 2, 1, 1, 0, 0, 0, 0},
	{"Amulet", "amu", "Amulet", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Top of the Horadric Staff", "vip", "Amulet", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ring", "rin", "Ring", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Gold", "gld", "Gold", 1, 1, 1, 0, 0, 0, 0, 0},
	{"Scroll of Inifuss", "bks", "Quest Item", 2, 2, 0, 0, 0, 0, 0, 0},
	{"Key to the Cairn Stones", "bkd", "Quest Item", 2, 2, 0, 0, 0, 0, 0, 0},
	{"Arrows", "aqv", "Arrows", 1, 3, 1, 0, 0, 0, 0, 0},
	{"Torch", "tch", "Torch", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Bolts", "cqv", "Bolts", 1, 3, 1, 0, 0, 0, 0, 0},
	{"Scroll of Town Portal", "tsc", "Scroll", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Scroll of Identify", "isc", "Scroll", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Heart", "hrt", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Brain", "brz", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Jawbone", "jaw", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Eye", "eyz", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Horn", "hrn", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Tail", "tal", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flag", "flg", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Fang", "fng", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Quill", "qll", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Soul", "sol", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Scalp", "scz", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Spleen", "spe", "Body Part", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Key", "key", "Key", 1, 1, 1, 0, 0, 0, 0, 0},
	{"The Black Tower Key", "luv", "Key", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Potion of Life", "xyz", "Quest Item", 1, 1, 0, 1, 0, 0, 0, 0},
	{"A Jade Figurine", "j34", "Quest Item", 1, 2, 0, 0, 0, 0, 0, 0},
	{"The Golden Bird", "g34", "Quest Item", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Lam Esen's Tome", "bbb", "Quest Item", 2, 2, 0, 0, 0, 0, 0, 0},
	{"Horadric Cube", "box", "Quest Item", 2, 2, 0, 1, 0, 0, 0, 0},
	{"Horadric Scroll", "tr1", "Quest Item", 2, 2, 0, 0, 0, 0, 0, 0},
	{"Mephisto's Soulstone", "mss", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Book of Skill", "ass", "Quest Item", 2, 2, 0, 1, 0, 0, 0, 0},
	{"Khalim's Eye", "qey", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Khalim's Heart", "qhr", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Khalim's Brain", "qbr", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ear", "ear", "Ear", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Chipped Amethyst", "gcv", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawed Amethyst", "gfv", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Amethyst", "gsv", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawless Amethyst", "gzv", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Perfect Amethyst", "gpv", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Chipped Topaz", "gcy", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawed Topaz", "gfy", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Topaz", "gsy", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawless Topaz", "gly", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Perfect Topaz", "gpy", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Chipped Sapphire", "gcb", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawed Sapphire", "gfb", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Sapphire", "gsb", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawless Sapphire", "glb", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Perfect Sapphire", "gpb", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Chipped Emerald", "gcg", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawed Emerald", "gfg", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Emerald", "gsg", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawless Emerald", "glg", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Perfect Emerald", "gpg", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Chipped Ruby", "gcr", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawed Ruby", "gfr", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ruby", "gsr", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawless Ruby", "glr", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Perfect Ruby", "gpr", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Chipped Diamond", "gcw", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawed Diamond", "gfw", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Diamond", "gsw", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawless Diamond", "glw", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Perfect Diamond", "gpw", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Minor Healing Potion", "hp1", "Health Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Light Healing Potion", "hp2", "Health Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Healing Potion", "hp3", "Health Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Greater Healing Potion", "hp4", "Health Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Super Healing Potion", "hp5", "Health Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Minor Mana Potion", "mp1", "Mana Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Light Mana Potion", "mp2", "Mana Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Mana Potion", "mp3", "Mana Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Greater Mana Potion", "mp4", "Mana Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Super Mana Potion", "mp5", "Mana Potion", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Chipped Skull", "skc", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawed Skull", "skf", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Skull", "sku", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Flawless Skull", "skl", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Perfect Skull", "skz", "Gem", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Herb", "hrb", "Herb", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Small Charm", "cm1", "Small Charm", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Large Charm", "cm2", "Large Charm", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Grand Charm", "cm3", "Grand Charm", 1, 3, 0, 0, 0, 0, 0, 0},
	{"rps", "rps", "Health Potion", 1, 1, 1, 1, 0, 0, 0, 0},
	{"rpl", "rpl", "Health Potion", 1, 1, 1, 1, 0, 0, 0, 0},
	{"bps", "bps", "Mana Potion", 1, 1, 1, 1, 0, 0, 0, 0},
	{"bpl", "bpl", "Mana Potion", 1, 1, 1, 1, 0, 0, 0, 0},
	{"El Rune", "r01", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Eld Rune", "r02", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Tir Rune", "r03", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Nef Rune", "r04", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Eth Rune", "r05", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ith Rune", "r06", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Tal Rune", "r07", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ral Rune", "r08", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ort Rune", "r09", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Thul Rune", "r10", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Amn Rune", "r11", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Sol Rune", "r12", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Shael Rune", "r13", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Dol Rune", "r14", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Hel Rune", "r15", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Io Rune", "r16", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Lum Rune", "r17", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ko Rune", "r18", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Fal Rune", "r19", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Lem Rune", "r20", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Pul Rune", "r21", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Um Rune", "r22", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Mal Rune", "r23", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ist Rune", "r24", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Gul Rune", "r25", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Vex Rune", "r26", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ohm Rune", "r27", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Lo Rune", "r28", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Sur Rune", "r29", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Ber Rune", "r30", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Jah Rune", "r31", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Cham Rune", "r32", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Zod Rune", "r33", "Rune", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Jewel", "jew", "Jewel", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Malah's Potion", "ice", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Scroll of Knowledge", "0sc", "Scroll", 1, 1, 0, 1, 0, 0, 0, 0},
	{"Scroll of Resistance", "tr2", "Quest Item", 2, 2, 0, 1, 0, 0, 0, 0},
	{"Key of Terror", "pk1", "Quest Item", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Key of Hate", "pk2", "Quest Item", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Key of Destruction", "pk3", "Quest Item", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Diablo's Horn", "dhn", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Baal's Eye", "bey", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Mephisto's Brain", "mbr", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Standard of Heroes", "std", "Quest Item", 1, 1, 0, 0, 0, 0, 0, 0},
	{"Hand Axe", "hax", "Axe", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Axe", "axe", "Axe", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Double Axe", "2ax", "Axe", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Military Pick", "mpi", "Axe", 2, 3, 0, 0, 0, 1, 0, 0},
	{"War Axe", "wax", "Axe", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Large Axe", "lax", "Axe", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Broad Axe", "bax", "Axe", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Battle Axe", "btx", "Axe", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Great Axe", "gax", "Axe", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Giant Axe", "gix", "Axe", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Wand", "wnd", "Wand", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Yew Wand", "ywn", "Wand", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Bone Wand", "bwn", "Wand", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Grim Wand", "gwn", "Wand", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Club", "clb", "Club", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Scepter", "scp", "Scepter", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Grand Scepter", "gsc", "Scepter", 1, 3, 0, 0, 0, 1, 0, 0},
	{"War Scepter", "wsp", "Scepter", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Spiked Club", "spc", "Club", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Mace", "mac", "Mace", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Morning Star", "mst", "Mace", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Flail", "fla", "Mace", 2, 3, 0, 0, 0, 1, 0, 0},
	{"War Hammer", "whm", "Hammer", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Maul", "mau", "Hammer", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Great Maul", "gma", "Hammer", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Short Sword", "ssd", "Sword", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Scimitar", "scm", "Sword", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Sabre", "sbr", "Sword", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Falchion", "flc", "Sword", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Crystal Sword", "crs", "Sword", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Broad Sword", "bsd", "Sword", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Long Sword", "lsd", "Sword", 2, 3, 0, 0, 0, 1, 0, 0},
	{"War Sword", "wsd", "Sword", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Two-Handed Sword", "2hs", "Sword", 1, 4, 0, 0, 0, 1, 0, 0},
	{"Claymore", "clm", "Sword", 1, 4, 0, 0, 0, 1, 0, 0},
	{"Giant Sword", "gis", "Sword", 1, 4, 0, 0, 0, 1, 0, 0},
	{"Bastard Sword", "bsw", "Sword", 1, 4, 0, 0, 0, 1, 0, 0},
	{"Flamberge", "flb", "Sword", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Great Sword", "gsd", "Sword", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Dagger", "dgr", "Dagger", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Dirk", "dir", "Dagger", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Kris", "kri", "Dagger", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Blade", "bld", "Dagger", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Throwing Knife", "tkf", "Throwing Knife", 1, 2, 1, 0, 1, 1, 0, 0},
	{"Throwing Axe", "tax", "Throwing Axe", 1, 2, 1, 0, 1, 1, 0, 0},
	{"Balanced Knife", "bkf", "Throwing Knife", 1, 2, 1, 0, 1, 1, 0, 0},
	{"Balanced Axe", "bal", "Throwing Axe", 2, 3, 1, 0, 1, 1, 0, 0},
	{"Javelin", "jav", "Javelin", 1, 3, 1, 0, 1, 1, 0, 0},
	{"Pilum", "pil", "Javelin", 1, 3, 1, 0, 1, 1, 0, 0},
	{"Short Spear", "ssp", "Javelin", 1, 3, 1, 0, 1, 1, 0, 0},
	{"Glaive", "glv", "Javelin", 1, 4, 1, 0, 1, 1, 0, 0},
	{"Throwing Spear", "tsp", "Javelin", 1, 4, 1, 0, 1, 1, 0, 0},
	{"Spear", "spr", "Spear", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Trident", "tri", "Spear", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Brandistock", "brn", "Spear", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Spetum", "spt", "Spear", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Pike", "pik", "Spear", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Bardiche", "bar", "Polearm", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Voulge", "vou", "Polearm", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Scythe", "scy", "Polearm", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Poleaxe", "pax", "Polearm", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Halberd", "hal", "Polearm", 2, 4, 0, 0, 0, 1, 0, 0},
	{"War Scythe", "wsc", "Polearm", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Short Staff", "sst", "Staff", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Long Staff", "lst", "Staff", 1, 4, 0, 0, 0, 1, 0, 0},
	{"Gnarled Staff", "cst", "Staff", 1, 4, 0, 0, 0, 1, 0, 0},
	{"Battle Staff", "bst", "Staff", 1, 4, 0, 0, 0, 1, 0, 0},
	{"War Staff", "wst", "Staff", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Short Bow", "sbw", "Bow", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Hunter's Bow", "hbw", "Bow", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Long Bow", "lbw", "Bow", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Composite Bow", "cbw", "Bow", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Short Battle Bow", "sbb", "Bow", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Long Battle Bow", "lbb", "Bow", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Short War Bow", "swb", "Bow", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Long War Bow", "lwb", "Bow", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Light Crossbow", "lxb", "Crossbow", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Crossbow", "mxb", "Crossbow", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Heavy Crossbow", "hxb", "Crossbow", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Repeating Crossbow", "rxb", "Crossbow", 2, 3, 0, 0, 0, 1, 0, 0},
	{"Rancid Gas Potion", "gps", "Throwing Potion", 1, 1, 1, 0, 0, 0, 0, 0},
	{"Oil Potion", "ops", "Throwing Potion", 1, 1, 1, 0, 0, 0, 0, 0},
	{"Choking Gas Potion", "gpm", "Throwing Potion", 1, 1, 1, 0, 0, 0, 0, 0},
	{"Exploding Potion", "opm", "Throwing Potion", 1, 1, 1, 0, 0, 0, 0, 0},
	{"Strangling Gas Potion", "gpl", "Throwing Potion", 1, 1, 1, 0, 0, 0, 0, 0},
	{"Fulminating Potion", "opl", "Throwing Potion", 1, 1, 1, 0, 0, 0, 0, 0},
	{"Decoy Gidbinn", "d33", "Dagger", 1, 2, 0, 0, 0, 0, 0, 0},
	{"The Gidbinn", "g33", "Dagger", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Wirt's Leg", "leg", "Club", 1, 3, 0, 0, 0, 0, 0, 0},
	{"Horadric Malus", "hdm", "Hammer", 1, 2, 0, 0, 0, 0, 0, 0},
	{"Hell Forge Hammer", "hfh", "Hammer", 2, 3, 0, 0, 0, 0, 0, 0},
	{"Horadric Staff", "hst", "Staff", 1, 4, 0, 0, 0, 0, 0, 0},
	{"Shaft of the Horadric Staff", "msf", "Staff", 1, 3, 0, 0, 0, 0, 0, 0},
	{"Hatchet", "9ha", "Axe", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Cleaver", "9ax", "Axe", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Twin Axe", "92a", "Axe", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Crowbill", "9mp", "Axe", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Naga", "9wa", "Axe", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Military Axe", "9la", "Axe", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Bearded Axe", "9ba", "Axe", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Tabar", "9bt", "Axe", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Gothic Axe", "9ga", "Axe", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Ancient Axe", "9gi", "Axe", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Burnt Wand", "9wn", "Wand", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Petrified Wand", "9yw", "Wand", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Tomb Wand", "9bw", "Wand", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Grave Wand", "9gw", "Wand", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Cudgel", "9cl", "Club", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Rune Scepter", "9sc", "Scepter", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Holy Water Sprinkler", "9qs", "Scepter", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Divine Scepter", "9ws", "Scepter", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Barbed Club", "9sp", "Club", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Flanged Mace", "9ma", "Mace", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Jagged Star", "9mt", "Mace", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Knout", "9fl", "Mace", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Battle Hammer", "9wh", "Hammer", 2, 3, 0, 0, 0, 2, 0, 0},
	{"War Club", "9m9", "Hammer", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Martel de Fer", "9gm", "Hammer", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Gladius", "9ss", "Sword", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Cutlass", "9sm", "Sword", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Shamshir", "9sb", "Sword", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Tulwar", "9fc", "Sword", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Dimensional Blade", "9cr", "Sword", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Battle Sword", "9bs", "Sword", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Rune Sword", "9ls", "Sword", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Ancient Sword", "9wd", "Sword", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Espandon", "92h", "Sword", 1, 4, 0, 0, 0, 2, 0, 0},
	{"Dacian Falx", "9cm", "Sword", 1, 4, 0, 0, 0, 2, 0, 0},
	{"Tusk Sword", "9gs", "Sword", 1, 4, 0, 0, 0, 2, 0, 0},
	{"Gothic Sword", "9b9", "Sword", 1, 4, 0, 0, 0, 2, 0, 0},
	{"Zweihander", "9fb", "Sword", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Executioner Sword", "9gd", "Sword", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Poignard", "9dg", "Dagger", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Rondel", "9di", "Dagger", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Cinquedeas", "9kr", "Dagger", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Stiletto", "9bl", "Dagger", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Battle Dart", "9tk", "Throwing Knife", 1, 2, 1, 0, 1, 2, 0, 0},
	{"Francisca", "9ta", "Throwing Axe", 1, 2, 1, 0, 1, 2, 0, 0},
	{"War Dart", "9bk", "Throwing Knife", 1, 2, 1, 0, 1, 2, 0, 0},
	{"Hurlbat", "9b8", "Throwing Axe", 2, 3, 1, 0, 1, 2, 0, 0},
	{"War Javelin", "9ja", "Javelin", 1, 3, 1, 0, 1, 2, 0, 0},
	{"Great Pilum", "9pi", "Javelin", 1, 3, 1, 0, 1, 2, 0, 0},
	{"Simbilan", "9s9", "Javelin", 1, 3, 1, 0, 1, 2, 0, 0},
	{"Spiculum", "9gl", "Javelin", 1, 4, 1, 0, 1, 2, 0, 0},
	{"Harpoon", "9ts", "Javelin", 1, 4, 1, 0, 1, 2, 0, 0},
	{"War Spear", "9sr", "Spear", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Fuscina", "9tr", "Spear", 2, 4, 0, 0, 0, 2, 0, 0},
	{"War Fork", "9br", "Spear", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Yari", "9st", "Spear", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Lance", "9p9", "Spear", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Lochaber Axe", "9b7", "Polearm", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Bill", "9vo", "Polearm", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Battle Scythe", "9s8", "Polearm", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Partizan", "9pa", "Polearm", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Bec-de-Corbin", "9h9", "Polearm", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Grim Scythe", "9wc", "Polearm", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Jo Staff", "8ss", "Staff", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Quarterstaff", "8ls", "Staff", 1, 4, 0, 0, 0, 2, 0, 0},
	{"Cedar Staff", "8cs", "Staff", 1, 4, 0, 0, 0, 2, 0, 0},
	{"Gothic Staff", "8bs", "Staff", 1, 4, 0, 0, 0, 2, 0, 0},
	{"Rune Staff", "8ws", "Staff", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Edge Bow", "8sb", "Bow", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Razor Bow", "8hb", "Bow", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Cedar Bow", "8lb", "Bow", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Double Bow", "8cb", "Bow", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Short Siege Bow", "8s8", "Bow", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Large Siege Bow", "8l8", "Bow", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Rune Bow", "8sw", "Bow", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Gothic Bow", "8lw", "Bow", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Arbalest", "8lx", "Crossbow", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Siege Crossbow", "8mx", "Crossbow", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Ballista", "8hx", "Crossbow", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Chu-Ko-Nu", "8rx", "Crossbow", 2, 3, 0, 0, 0, 2, 0, 0},
	{"Khalim's Flail", "qf1", "Mace", 2, 3, 0, 0, 0, 0, 0, 0},
	{"Khalim's Will", "qf2", "Mace", 2, 3, 0, 0, 0, 0, 0, 0},
	{"Katar", "ktr", "Assassin Katar", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Wrist Blade", "wrb", "Assassin Katar", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Hatchet Hands", "axf", "Assassin Katar", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Cestus", "ces", "Assassin Katar", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Claws", "clw", "Assassin Katar", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Blade Talons", "btl", "Assassin Katar", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Scissors Katar", "skr", "Assassin Katar", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Quhab", "9ar", "Assassin Katar", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Wrist Spike", "9wb", "Assassin Katar", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Fascia", "9xf", "Assassin Katar", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Hand Scythe", "9cs", "Assassin Katar", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Greater Claws", "9lw", "Assassin Katar", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Greater Talons", "9tw", "Assassin Katar", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Scissors Quhab", "9qr", "Assassin Katar", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Suwayyah", "7ar", "Assassin Katar", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Wrist Sword", "7wb", "Assassin Katar", 1, 3, 0, 0, 0, 3, 0, 0},
	{"War Fist", "7xf", "Assassin Katar", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Battle Cestus", "7cs", "Assassin Katar", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Feral Claws", "7lw", "Assassin Katar", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Runic Talons", "7tw", "Assassin Katar", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Scissors Suwayyah", "7qr", "Assassin Katar", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Tomahawk", "7ha", "Axe", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Small Crescent", "7ax", "Axe", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Ettin Axe", "72a", "Axe", 2, 3, 0, 0, 0, 3, 0, 0},
	{"War Spike", "7mp", "Axe", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Berserker Axe", "7wa", "Axe", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Feral Axe", "7la", "Axe", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Silver-edged Axe", "7ba", "Axe", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Decapitator", "7bt", "Axe", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Champion Axe", "7ga", "Axe", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Glorious Axe", "7gi", "Axe", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Polished Wand", "7wn", "Wand", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Ghost Wand", "7yw", "Wand", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Lich Wand", "7bw", "Wand", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Unearthed Wand", "7gw", "Wand", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Truncheon", "7cl", "Club", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Mighty Scepter", "7sc", "Scepter", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Seraph Rod", "7qs", "Scepter", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Caduceus", "7ws", "Scepter", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Tyrant Club", "7sp", "Club", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Reinforced Mace", "7ma", "Mace", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Devil Star", "7mt", "Mace", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Scourge", "7fl", "Mace", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Legendary Mallet", "7wh", "Hammer", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Ogre Maul", "7m7", "Hammer", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Thunder Maul", "7gm", "Hammer", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Falcata", "7ss", "Sword", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Ataghan", "7sm", "Sword", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Elegant Blade", "7sb", "Sword", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Hydra Edge", "7fc", "Sword", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Phase Blade", "7cr", "Sword", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Conquest Sword", "7bs", "Sword", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Cryptic Sword", "7ls", "Sword", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Mythical Sword", "7wd", "Sword", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Legend Sword", "72h", "Sword", 1, 4, 0, 0, 0, 3, 0, 0},
	{"Highland Blade", "7cm", "Sword", 1, 4, 0, 0, 0, 3, 0, 0},
	{"Balrog Blade", "7gs", "Sword", 1, 4, 0, 0, 0, 3, 0, 0},
	{"Champion Sword", "7b7", "Sword", 1, 4, 0, 0, 0, 3, 0, 0},
	{"Colossus Sword", "7fb", "Sword", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Colossus Blade", "7gd", "Sword", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Bone Knife", "7dg", "Dagger", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Mithril Point", "7di", "Dagger", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Fanged Knife", "7kr", "Dagger", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Legend Spike", "7bl", "Dagger", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Flying Knife", "7tk", "Throwing Knife", 1, 2, 1, 0, 1, 3, 0, 0},
	{"Flying Axe", "7ta", "Throwing Axe", 1, 2, 1, 0, 1, 3, 0, 0},
	{"Winged Knife", "7bk", "Throwing Knife", 1, 2, 1, 0, 1, 3, 0, 0},
	{"Winged Axe", "7b8", "Throwing Axe", 2, 3, 1, 0, 1, 3, 0, 0},
	{"Hyperion Javelin", "7ja", "Javelin", 1, 3, 1, 0, 1, 3, 0, 0},
	{"Stygian Pilum", "7pi", "Javelin", 1, 3, 1, 0, 1, 3, 0, 0},
	{"Balrog Spear", "7s7", "Javelin", 1, 3, 1, 0, 1, 3, 0, 0},
	{"Ghost Glaive", "7gl", "Javelin", 1, 4, 1, 0, 1, 3, 0, 0},
	{"Winged Harpoon", "7ts", "Javelin", 1, 4, 1, 0, 1, 3, 0, 0},
	{"Hyperion Spear", "7sr", "Spear", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Stygian Pike", "7tr", "Spear", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Mancatcher", "7br", "Spear", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Ghost Spear", "7st", "Spear", 2, 4, 0, 0, 0, 3, 0, 0},
	{"War Pike", "7p7", "Spear", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Ogre Axe", "7o7", "Polearm", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Colossus Voulge", "7vo", "Polearm", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Thresher", "7s8", "Polearm", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Cryptic Axe", "7pa", "Polearm", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Great Poleaxe", "7h7", "Polearm", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Giant Thresher", "7wc", "Polearm", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Walking Stick", "6ss", "Staff", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Stalagmite", "6ls", "Staff", 1, 4, 0, 0, 0, 3, 0, 0},
	{"Elder Staff", "6cs", "Staff", 1, 4, 0, 0, 0, 3, 0, 0},
	{"Shillelagh", "6bs", "Staff", 1, 4, 0, 0, 0, 3, 0, 0},
	{"Archon Staff", "6ws", "Staff", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Spider Bow", "6sb", "Bow", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Blade Bow", "6hb", "Bow", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Shadow Bow", "6lb", "Bow", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Great Bow", "6cb", "Bow", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Diamond Bow", "6s7", "Bow", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Crusader Bow", "6l7", "Bow", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Ward Bow", "6sw", "Bow", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Hydra Bow", "6lw", "Bow", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Pellet Bow", "6lx", "Crossbow", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Gorgon Crossbow", "6mx", "Crossbow", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Colossus Crossbow", "6hx", "Crossbow", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Demon Crossbow", "6rx", "Crossbow", 2, 3, 0, 0, 0, 3, 0, 0},
	{"Eagle Orb", "ob1", "Sorceress Orb", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Sacred Globe", "ob2", "Sorceress Orb", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Smoked Sphere", "ob3", "Sorceress Orb", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Clasped Orb", "ob4", "Sorceress Orb", 1, 2, 0, 0, 0, 1, 0, 0},
	{"Jared's Stone", "ob5", "Sorceress Orb", 1, 3, 0, 0, 0, 1, 0, 0},
	{"Stag Bow", "am1", "Amazon Bow", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Reflex Bow", "am2", "Amazon Bow", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Maiden Spear", "am3", "Amazon Spear", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Maiden Pike", "am4", "Amazon Spear", 2, 4, 0, 0, 0, 1, 0, 0},
	{"Maiden Javelin", "am5", "Amazon Javelin", 1, 3, 1, 0, 1, 1, 0, 0},
	{"Glowing Orb", "ob6", "Sorceress Orb", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Crystalline Globe", "ob7", "Sorceress Orb", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Cloudy Sphere", "ob8", "Sorceress Orb", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Sparkling Ball", "ob9", "Sorceress Orb", 1, 2, 0, 0, 0, 2, 0, 0},
	{"Swirling Crystal", "oba", "Sorceress Orb", 1, 3, 0, 0, 0, 2, 0, 0},
	{"Ashwood Bow", "am6", "Amazon Bow", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Ceremonial Bow", "am7", "Amazon Bow", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Ceremonial Spear", "am8", "Amazon Spear", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Ceremonial Pike", "am9", "Amazon Spear", 2, 4, 0, 0, 0, 2, 0, 0},
	{"Ceremonial Javelin", "ama", "Amazon Javelin", 1, 3, 1, 0, 1, 2, 0, 0},
	{"Heavenly Stone", "obb", "Sorceress Orb", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Eldritch Orb", "obc", "Sorceress Orb", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Demon Heart", "obd", "Sorceress Orb", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Vortex Orb", "obe", "Sorceress Orb", 1, 2, 0, 0, 0, 3, 0, 0},
	{"Dimensional Shard", "obf", "Sorceress Orb", 1, 3, 0, 0, 0, 3, 0, 0},
	{"Matriarchal Bow", "amb", "Amazon Bow", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Grand Matron Bow", "amc", "Amazon Bow", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Matriarchal Spear", "amd", "Amazon Spear", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Matriarchal Pike", "ame", "Amazon Spear", 2, 4, 0, 0, 0, 3, 0, 0},
	{"Matriarchal Javelin", "amf", "Amazon Javelin", 1, 3, 1, 0, 1, 3, 0, 0}
};

ItemPropertyBits ItemPropertyBitsList[] = {
	{"Strength", 8, 0, 32},
	{"Energy", 7, 0, 32},
	{"Dexterity", 7, 0, 32},
	{"Vitality", 7, 0, 32},
	{"All Attributes", 0, 0, 0},
	{"New Skills", 0, 0, 0},
	{"Life", 0, 0, 0},
	{"Maximum Life", 9, 0, 32},
	{"Mana", 0, 0, 0},
	{"Maximum Mana", 8, 0, 32},
	{"Stamina", 0, 0, 0},
	{"Maximum Stamina", 8, 0, 32},
	{"Level", 0, 0, 0},
	{"Experience", 0, 0, 0},
	{"Gold", 0, 0, 0},
	{"Bank", 0, 0, 0},
	{"Enhanced Defense", 9, 0, 0},
	{"Enhanced Maximum Damage", 9, 0, 0},
	{"Enhanced Minimum Damage", 9, 0, 0},
	{"Attack Rating", 10, 0, 0},
	{"Increased Blocking", 6, 0, 0},
	{"Minimum Damage", 6, 0, 0},
	{"Maximum Damage", 7, 0, 0},
	{"Secondary Minimum Damage", 6, 0, 0},
	{"Secondary Maximum Damage", 7, 0, 0},
	{"Enhanced Damage", 8, 0, 0},
	{"Mana Recovery", 8, 0, 0},
	{"Mana Recovery Bonus", 8, 0, 0},
	{"Stamina Recovery Bonus", 8, 0, 0},
	{"Last Experience", 0, 0, 0},
	{"Next Experience", 0, 0, 0},
	{"Defense", 11, 0, 10},
	{"Defense vs. Missiles", 9, 0, 0},
	{"Defense vs. Melee", 8, 0, 0},
	{"Damage Reduction", 6, 0, 0},
	{"Magical Damage Reduction", 6, 0, 0},
	{"Damage Reduction (Percent)", 8, 0, 0},
	{"Magical Damage Reduction (Percent)", 8, 0, 0},
	{"Maximum Magical Damage Reduction (Percent)", 5, 0, 0},
	{"Fire Resistance", 8, 0, 50},
	{"Maximum Fire Resistance", 5, 0, 0},
	{"Lightning Resistance", 8, 0, 50},
	{"Maximum Lightning Resistance", 5, 0, 0},
	{"Cold Resistance", 8, 0, 50},
	{"Maximum Cold Resistance", 5, 0, 0},
	{"Poison Resistance", 8, 0, 50},
	{"Maximum Poison Resistance", 5, 0, 0},
	{"Damage Aura", 0, 0, 0},
	{"Minimum Fire Damage", 8, 0, 0},
	{"Maximum Fire Damage", 9, 0, 0},
	{"Minimum Lightning Damage", 6, 0, 0},
	{"Maximum Lightning Damage", 10, 0, 0},
	{"Minimum Magical Damage", 8, 0, 0},
	{"Maximum Magical Damage", 9, 0, 0},
	{"Minimum Cold Damage", 8, 0, 0},
	{"Maximum Cold Damage", 9, 0, 0},
	{"Cold Damage Length", 8, 0, 0},
	{"Minimum Poison Damage", 10, 0, 0},
	{"Maximum Poison Damage", 10, 0, 0},
	{"Poison Damage length", 9, 0, 0},
	{"Minimum Life Stolen Per Hit", 7, 0, 0},
	{"Maximum Life Stolen Per Hit", 0, 0, 0},
	{"Minimum Mana Stolen Per Hit", 7, 0, 0},
	{"Maximum Mana Stolen Per Hit", 0, 0, 0},
	{"Minimum Stamina Drain", 0, 0, 0},
	{"Maximum Stamina Drain", 0, 0, 0},
	{"Stun Length", 0, 0, 0},
	{"Velocity Percent", 7, 0, 30},
	{"Attack Rate", 7, 0, 30},
	{"Other Animation Rate", 0, 0, 0},
	{"Quantity", 0, 0, 0},
	{"Value", 8, 0, 100},
	{"Durability", 9, 0, 0},
	{"Maximum Durability", 8, 0, 0},
	{"Replenish Life", 6, 0, 30},
	{"Enhanced Maximum Durability", 7, 0, 20},
	{"Enhanced Life", 6, 0, 10},
	{"Enhanced Mana", 6, 0, 10},
	{"Attacker Takes Damage", 7, 0, 0},
	{"Extra Gold", 9, 0, 100},
	{"Better Chance Of Getting Magic Item", 8, 0, 100},
	{"Knockback", 7, 0, 0},
	{"Time Duration", 9, 0, 20},
	{"Class Skills", 3, 3, 0},
	{"Unsent Parameter", 0, 0, 0},
	{"Add Experience", 9, 0, 50},
	{"Life After Each Kill", 7, 0, 0},
	{"Reduce Vendor Prices", 7, 0, 0},
	{"Double Herb Duration", 1, 0, 0},
	{"Light Radius", 4, 0, 4},
	{"Light Colour", 24, 0, 0},
	{"Reduced Requirements", 8, 0, 100},
	{"Reduced Level Requirement", 7, 0, 0},
	{"Increased Attack Speed", 7, 0, 20},
	{"Reduced Level Requirement (Percent)", 7, 0, 64},
	{"Last Block Frame", 0, 0, 0},
	{"Faster Run Walk", 7, 0, 20},
	{"Non Class Skill", 6, 9, 0},
	{"State", 1, 8, 0},
	{"Faster Hit Recovery", 7, 0, 20},
	{"Monster Player Count", 0, 0, 0},
	{"Skill Poison Override Length", 0, 0, 0},
	{"Faster Block Rate", 7, 0, 20},
	{"Skill Bypass Undead", 0, 0, 0},
	{"Skill Bypass Demons", 0, 0, 0},
	{"Faster Cast Rate", 7, 0, 20},
	{"Skill Bypass Beasts", 0, 0, 0},
	{"Single Skill", 3, 9, 0},
	{"Slain Monsters Rest In Peace", 1, 0, 0},
	{"Curse Resistance", 9, 0, 0},
	{"Poison Length Reduction", 8, 0, 20},
	{"Adds Damage", 9, 0, 20},
	{"Hit Causes Monster To Flee", 7, 0, -1},
	{"Hit Blinds Target", 7, 0, 0},
	{"Damage To Mana", 6, 0, 0},
	{"Ignore Target's Defense", 1, 0, 0},
	{"Reduce Target's Defense", 7, 0, 0},
	{"Prevent Monster Heal", 7, 0, 0},
	{"Half Freeze Duration", 1, 0, 0},
	{"To Hit Percent", 9, 0, 20},
	{"Monster Defense Reduction per Hit", 7, 0, 128},
	{"Damage To Demons", 9, 0, 20},
	{"Damage To Undead", 9, 0, 20},
	{"Attack Rating Against Demons", 10, 0, 128},
	{"Attack Rating Against Undead", 10, 0, 128},
	{"Throwable", 1, 0, 0},
	{"Elemental Skills", 3, 3, 0},
	{"All Skills", 3, 0, 0},
	{"Attacker Takes Lightning Damage", 5, 0, 0},
	{"Iron Maiden Level", 0, 0, 0},
	{"Lifetap Level", 0, 0, 0},
	{"Thorns Percent", 0, 0, 0},
	{"Bone Armor", 0, 0, 0},
	{"Maximum Bone Armor", 0, 0, 0},
	{"Freezes Target", 5, 0, 0},
	{"Open Wounds", 7, 0, 0},
	{"Crushing Blow", 7, 0, 0},
	{"Kick Damage", 7, 0, 0},
	{"Mana After Each Kill", 7, 0, 0},
	{"Life After Each Demon Kill", 7, 0, 0},
	{"Extra Blood", 7, 0, 0},
	{"Deadly Strike", 7, 0, 0},
	{"Fire Absorption (Percent)", 7, 0, 0},
	{"Fire Absorption", 7, 0, 0},
	{"Lightning Absorption (Percent)", 7, 0, 0},
	{"Lightning Absorption", 7, 0, 0},
	{"Magic Absorption (Percent)", 7, 0, 0},
	{"Magic Absorption", 7, 0, 0},
	{"Cold Absorption (Percent)", 7, 0, 0},
	{"Cold Absorption", 7, 0, 0},
	{"Slows Down Enemies", 7, 0, 0},
	{"Aura", 5, 9, 0},
	{"Indestructible", 1, 0, 0},
	{"Cannot Be Frozen", 1, 0, 0},
	{"Stamina Drain (Percent)", 7, 0, 20},
	{"Reanimate", 7, 10, 0},
	{"Piercing Attack", 7, 0, 0},
	{"Fires Magic Arrows", 7, 0, 0},
	{"Fire Explosive Arrows", 7, 0, 0},
	{"Minimum Throwing Damage", 6, 0, 0},
	{"Maximum Throwing Damage", 7, 0, 0},
	{"Skill Hand Of Athena", 0, 0, 0},
	{"Skill Stamina (Percent)", 0, 0, 0},
	{"Skill Passive Stamina (Percent)", 0, 0, 0},
	{"Concentration", 0, 0, 0},
	{"Enchant", 0, 0, 0},
	{"Pierce", 0, 0, 0},
	{"Conviction", 0, 0, 0},
	{"Chilling Armor", 0, 0, 0},
	{"Frenzy", 0, 0, 0},
	{"Decrepify", 0, 0, 0},
	{"Skill Armor Percent", 0, 0, 0},
	{"Alignment", 0, 0, 0},
	{"Target 0", 0, 0, 0},
	{"Target 1", 0, 0, 0},
	{"Gold Lost", 0, 0, 0},
	{"Conversion Level", 0, 0, 0},
	{"Conversion Maximum Life", 0, 0, 0},
	{"Unit Do Overlay", 0, 0, 0},
	{"Attack Rating Against Monster Type", 9, 10, 0},
	{"Damage To Monster Type", 9, 10, 0},
	{"Fade", 3, 0, 0},
	{"Armor Override Percent", 0, 0, 0},
	{"Unused 183", 0, 0, 0},
	{"Unused 184", 0, 0, 0},
	{"Unused 185", 0, 0, 0},
	{"Unused 186", 0, 0, 0},
	{"Unused 187", 0, 0, 0},
	{"Skill Tab", 3, 16, 0},
	{"Unused 189", 0, 0, 0},
	{"Unused 190", 0, 0, 0},
	{"Unused 191", 0, 0, 0},
	{"Unused 192", 0, 0, 0},
	{"Unused 193", 0, 0, 0},
	{"Socket Count", 4, 0, 0},
	{"Skill On Striking", 7, 16, 0},
	{"Skill On Kill", 7, 16, 0},
	{"Skill On Death", 7, 16, 0},
	{"Skill On Hit", 7, 16, 0},
	{"Skill On Level Up", 7, 16, 0},
	{"Unused 200", 0, 0, 0},
	{"Skill When Struck", 7, 16, 0},
	{"Unused 202", 0, 0, 0},
	{"Unused 203", 0, 0, 0},
	{"Charged", 16, 16, 0},
	{"Unused 204", 0, 0, 0},
	{"Unused 205", 0, 0, 0},
	{"Unused 206", 0, 0, 0},
	{"Unused 207", 0, 0, 0},
	{"Unused 208", 0, 0, 0},
	{"Unused 209", 0, 0, 0},
	{"Unused 210", 0, 0, 0},
	{"Unused 211", 0, 0, 0},
	{"Unused 212", 0, 0, 0},
	{"Defense Per Level", 6, 0, 0},
	{"Enhanced Defense Per Level", 6, 0, 0},
	{"Life Per Level", 6, 0, 0},
	{"Mana Per Level", 6, 0, 0},
	{"Maximum Damage Per Level", 6, 0, 0},
	{"Maximum Enhanced Damage Per Level", 6, 0, 0},
	{"Strength Per Level", 6, 0, 0},
	{"Dexterity Per Level", 6, 0, 0},
	{"Energy Per Level", 6, 0, 0},
	{"Vitality Per Level", 6, 0, 0},
	{"Attack Rating Per Level", 6, 0, 0},
	{"Bonus To Attack Rating Per Level", 6, 0, 0},
	{"Maximum Cold Damage Per Level", 6, 0, 0},
	{"Maximum Fire Damage Per Level", 6, 0, 0},
	{"Maximum Lightning Damage Per Level", 6, 0, 0},
	{"Maximum Poison Damage Per Level", 6, 0, 0},
	{"Cold Resistance Per Level", 6, 0, 0},
	{"Fire Resistance Per Level", 6, 0, 0},
	{"Lightning Resistance Per Level", 6, 0, 0},
	{"Poison Resistance Per Level", 6, 0, 0},
	{"Cold Absorption Per Level", 6, 0, 0},
	{"Fire Absorption Per Level", 6, 0, 0},
	{"Lightning Absorption Per Level", 6, 0, 0},
	{"Poison Absorption Per Level", 6, 0, 0},
	{"Thorns Per Level", 5, 0, 0},
	{"Extra Gold Per Level", 6, 0, 0},
	{"Better Chance Of Getting Magic Item Per Level", 6, 0, 0},
	{"Stamina Regeneration Per Level", 6, 0, 0},
	{"Stamina Per Level", 6, 0, 0},
	{"Damage To Demons Per Level", 6, 0, 0},
	{"Damage To Undead Per Level", 6, 0, 0},
	{"Attack Rating Against Demons Per Level", 6, 0, 0},
	{"Attack Rating Against Undead Per Level", 6, 0, 0},
	{"Crushing Blow Per Level", 6, 0, 0},
	{"Open Wounds Per Level", 6, 0, 0},
	{"Kick Damage Per Level", 6, 0, 0},
	{"Deadly Strike Per Level", 6, 0, 0},
	{"Find Gems Per Level", 0, 0, 0},
	{"Repairs Durability", 6, 0, 0},
	{"Replenishes Quantity", 6, 0, 0},
	{"Increased Stack Size", 8, 0, 0},
	{"Find Item", 0, 0, 0},
	{"Slash Damage", 0, 0, 0},
	{"Slash Damage (Percent)", 0, 0, 0},
	{"Crush Damage", 0, 0, 0},
	{"Crush Damage (Percent)", 0, 0, 0},
	{"Thrust Damage", 0, 0, 0},
	{"Thrust Damage (Percent)", 0, 0, 0},
	{"Slash Damage Absorption", 0, 0, 0},
	{"Crush Damage Absorption", 0, 0, 0},
	{"Thrust Damage Absorption", 0, 0, 0},
	{"Slash Damage Absorption (Percent)", 0, 0, 0},
	{"Crush Damage Absorption (Percent)", 0, 0, 0},
	{"Thrust Damage Absorption (Percent)", 0, 0, 0},
	{"Defense Per Time", 22, 0, 0},
	{"Enhanced Defense Per Time", 22, 0, 0},
	{"Life Per Time", 22, 0, 0},
	{"Mana Per Time", 22, 0, 0},
	{"Maximum Damage Per Time", 22, 0, 0},
	{"Maximum Enhanced Damage Per Time", 22, 0, 0},
	{"Strength Per Time", 22, 0, 0},
	{"Dexterity Per Time", 22, 0, 0},
	{"Energy Per Time", 22, 0, 0},
	{"Vitality Per Time", 22, 0, 0},
	{"Attack Rating Per Time", 22, 0, 0},
	{"Chance To Hit Per Time", 22, 0, 0},
	{"Maximum Cold Damage Per Time", 22, 0, 0},
	{"Maximum Fire Damage Per Time", 22, 0, 0},
	{"Maximum Lightning Damage Per Time", 22, 0, 0},
	{"Maximum Damage Per Poison", 22, 0, 0},
	{"Cold Resistance Per Time", 22, 0, 0},
	{"Fire Resistance Per Time", 22, 0, 0},
	{"Lightning Resistance Per Time", 22, 0, 0},
	{"Poison Resistance Per Time", 22, 0, 0},
	{"Cold Absorption Per Time", 22, 0, 0},
	{"Fire Absorption Per Time", 22, 0, 0},
	{"Lightning Absorption Per Time", 22, 0, 0},
	{"Poison Absorption Per Time", 22, 0, 0},
	{"Extra Gold Per Time", 22, 0, 0},
	{"Better Chance Of Getting magic Item Per Time", 22, 0, 0},
	{"Regenerate Stamina Per Time", 22, 0, 0},
	{"Stamina Per Time", 22, 0, 0},
	{"Damage To Demons Per Time", 22, 0, 0},
	{"Damage To Undead Per Time", 22, 0, 0},
	{"Attack Rating Against Demons Per Time", 22, 0, 0},
	{"Attack Rating Against Undead Per Time", 22, 0, 0},
	{"Crushing Blow Per Time", 22, 0, 0},
	{"Open Wounds Per Time", 22, 0, 0},
	{"Kick Damage Per Time", 22, 0, 0},
	{"Deadly Strike Per Time", 22, 0, 0},
	{"Find Gems Per Time", 0, 0, 0},
	{"Enemy Cold Resistance Reduction", 8, 0, 50},
	{"Enemy Fire Resistance Reduction", 8, 0, 50},
	{"Enemy Lightning Resistance Reduction", 8, 0, 50},
	{"Enemy Poison Resistance Reduction", 8, 0, 50},
	{"Damage vs. Monsters", 0, 0, 0},
	{"Enhanced Damage vs. Monsters", 0, 0, 0},
	{"Attack Rating Against Monsters", 0, 0, 0},
	{"Bonus To Attack Rating Against Monsters", 0, 0, 0},
	{"Defense vs. Monsters", 0, 0, 0},
	{"Enhanced Defense vs. Monsters", 0, 0, 0},
	{"Fire Damage Length", 0, 0, 0},
	{"Minimum Fire Damage Length", 0, 0, 0},
	{"Maximum Fire Damage Length", 0, 0, 0},
	{"Progressive Damage", 0, 0, 0},
	{"Progressive Steal", 0, 0, 0},
	{"Progressive Other", 0, 0, 0},
	{"Progressive Fire", 0, 0, 0},
	{"Progressive Cold", 0, 0, 0},
	{"Progressive Lightning", 0, 0, 0},
	{"Extra Charges", 6, 0, 0},
	{"Progressive Attack Rating", 0, 0, 0},
	{"Poison Count", 0, 0, 0},
	{"Damage Framerate", 0, 0, 0},
	{"Pierce IDX", 0, 0, 0},
	{"Fire Mastery", 9, 0, 50},
	{"Lightning Mastery", 9, 0, 50},
	{"Cold Mastery", 9, 0, 50},
	{"Poison Mastery", 9, 0, 50},
	{"Passive Enemy Fire Resistance Reduction", 8, 0, 0},
	{"Passive Enemy Lightning Resistance Reduction", 8, 0, 0},
	{"Passive Enemy Cold Resistance Reduction", 8, 0, 0},
	{"Passive Enemy Poison Resistance Reduction", 8, 0, 0},
	{"Critical Strike", 8, 0, 0},
	{"Dodge", 7, 0, 0},
	{"Avoid", 7, 0, 0},
	{"Evade", 7, 0, 0},
	{"Warmth", 8, 0, 0},
	{"Melee Attack Rating Mastery", 8, 0, 0},
	{"Melee Damage Mastery", 8, 0, 0},
	{"Melee Critical Hit Mastery", 8, 0, 0},
	{"Thrown Weapon Attack Rating Mastery", 8, 0, 0},
	{"Thrown Weapon Damage Mastery", 8, 0, 0},
	{"Thrown Weapon Critical Hit Mastery", 8, 0, 0},
	{"Weapon Block", 8, 0, 0},
	{"Summon Resist", 8, 0, 0},
	{"Modifier List Skill", 0, 0, 0},
	{"Modifier List Level", 0, 0, 0},
	{"Last Sent Life Percent", 0, 0, 0},
	{"Source Unit Type", 0, 0, 0},
	{"Source Unit ID", 0, 0, 0},
	{"Short Parameter 1", 0, 0, 0},
	{"Quest Item Difficulty", 2, 0, 0},
	{"Passive Magical Damage Mastery", 9, 0, 50},
	{"Passive Magical Resistance Reduction", 8, 0, 0}
};

void CreateItemTable() {
	for (int n = 0; n < sizeof(ItemAttributeList) / sizeof(ItemAttributeList[0]); n++) {
		if (ItemAttributeList[n].category.compare("Helm") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_HELM | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Armor") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_ARMOR | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Shield") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_SHIELD | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Gloves") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_GLOVES | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Boots") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_BOOTS | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Belt") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_BELT | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Druid Pelt") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_DRUID_PELT | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Barbarian Helm") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_BARBARIAN_HELM | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Paladin Shield") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_PALADIN_SHIELD | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Necromancer Shrunken Head") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_NECROMANCER_SHIELD | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Circlet") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_CIRCLET | ITEM_GROUP_ALLARMOR;
		} else if (ItemAttributeList[n].category.compare("Axe") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_AXE | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Mace") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_MACE | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Sword") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_SWORD | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Dagger") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_DAGGER | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Throwing Axe") == 0 || ItemAttributeList[n].category.compare("Throwing Knife") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_THROWING | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Javelin") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_JAVELIN | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Spear") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_SPEAR | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Polearm") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_POLEARM | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Bow") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_BOW | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Crossbow") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_CROSSBOW | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Staff") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_STAFF | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Wand") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_WAND | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Scepter") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_SCEPTER | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Assassin Katar") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_ASSASSIN_KATAR | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Sorceress Orb") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_SORCERESS_ORB | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare(0, 6, "Amazon") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_AMAZON_WEAPON | ITEM_GROUP_ALLWEAPON;
		} else if (ItemAttributeList[n].category.compare("Throwing Potion") == 0) {
			ItemAttributeList[n].flags |= ITEM_GROUP_ALLWEAPON;
		}

		if (ItemAttributeList[n].itemLevel == 1) {
			ItemAttributeList[n].flags |= ITEM_GROUP_NORMAL;
		} else if (ItemAttributeList[n].itemLevel == 2) {
			ItemAttributeList[n].flags |= ITEM_GROUP_EXCEPTIONAL;
		} else if (ItemAttributeList[n].itemLevel == 3) {
			ItemAttributeList[n].flags |= ITEM_GROUP_ELITE;
		}

		string itemCode(ItemAttributeList[n].code);
		ItemAttributeMap[itemCode] = &ItemAttributeList[n];
	}
}

int GetDefense(ItemInfo *item) {
	int def = item->defense;
	for (vector<ItemProperty>::iterator prop = item->properties.begin(); prop < item->properties.end(); prop++) {
		if (prop->stat == STAT_ENHANCEDDEFENSE) {
			def *= (prop->value + 100);
			def /= 100;
		}
	}
	return def;
}
