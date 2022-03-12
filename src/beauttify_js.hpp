/**
 *  File        :   beautify_js.hpp
 *  Explain     :   C++ javascript beautifier implementation.
 *  Author      :   Mehmet Ekemen
 *  Nickname    :   `hun
 *  Email       :   ekemenms@hotmail.com
 *  Date        :   12.03.2022 (DD-MM-YYYY)  (Created)
 *  Github      :   github.com/hun756
 *  Resource    :   https://github.com/hun756/Javascript-Beautifier
**/

#ifndef JS_BEAUTIFIER_HPP
#define JS_BEAUTIFIER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <windows.h>

typedef std::wstring String;

namespace StringHelper 
{
    /**
     * @brief 
     * 
     * @param source 
     * @param delimiter 
     * @return std::vector<String> 
     */
    static std::vector<String> split(const String &source, wchar_t delimiter)
	{
		std::vector<String> output;
		std::wistringstream ss(source);
		String nextItem;

		while (std::getline(ss, nextItem, delimiter))
		{
			output.push_back(nextItem);
		}

		return output;
	}

    /**
     * @brief 
     * 
     * @tparam T 
     * @param subject 
     * @return std::wstring 
     */
    template<typename T>
	static std::wstring toString(const T &subject)
	{
		std::wostringstream ss;
		ss << subject;
		return ss.str();
	}
}

namespace JsBeautify 
{
    /**
     * @brief 
     * 
     */
    enum class BraceStyle
	{
		Expand,
		Collapse,
		EndExpand
	};

    /**
     * @brief 
     * 
     */
    struct BeautifierOptions
    {
        uint32_t    indentSize;
        wchar_t     indentChar;
        bool        indentWithTabs;
        bool        preserveNewLines;
        bool        jsLintHappy;
        BraceStyle  braceStyle;
        bool        keepArrayIndentation;
		bool        keepFunctionIndentation;
		bool        evalCode;
		int         wrapLineLength;
		bool        breakChainedMethods;

        /**
         * @brief Construct a new Beautifier Options object
         * 
         */
        BeautifierOptions() 
            :   indentSize(0),
                indentChar(L'\0'),
                indentWithTabs(false),
                preserveNewLines(false),
                jsLintHappy(false),
                braceStyle(BraceStyle::Expand),
                keepArrayIndentation(false),
                keepFunctionIndentation(false),
                evalCode(false),
                wrapLineLength(0),
                breakChainedMethods(false)
        { }
    };

    /**
     * @brief 
     * 
     */
    struct BeautifierFlags
    {
		String  previousMode;
		String  mode;
		bool    varLine = false;
		bool    varLineTainted = false;
		bool    varLineReindented = false;
		bool    inHtmlComment = false;
		bool    ifLine = false;
		int     chainExtraIndentation = 0;
		bool    inCase = false;
		bool    inCaseStatement = false;
		bool    caseBody = false;
		int     indentationLevel = 0;
		int     ternaryDepth = 0;

        BeautifierFlags(const String& mode = L"") 
            :   previousMode(L"BLOCK"),
                mode(mode),
                varLine(false),
                varLineReindented(false),
                inHtmlComment(false),
                ifLine(false),
                chainExtraIndentation(0),
                inCase(false),
                inCaseStatement(false),
                caseBody(false),
                indentationLevel(0),
                ternaryDepth(0)
        { }
    };
}

namespace JsBeautify 
{
    class Beautifier
    {
    public:
        /**
         * @brief Construct a new Beautifier object
         * 
         */
        Beautifier() : impl(std::make_unique<Impl>()) 
        {}

        Beautifier(const BeautifierOptions& opts) 
            : impl(std::make_unique<Impl>())
        {
            impl->opts = opts;

            BeautifierFlags tempVar (L"BLOCK");
			impl->flags = tempVar;
			impl->flagStore = std::vector<BeautifierFlags>();
			impl->wantedNewline = false;
			impl->justAddedNewline = false;
			impl->doBlockJustClosed = false;

			(getOpts().indentWithTabs) ?
				setIndentString(L"\t") :
				setIndentString (
                    String(getOpts().indentChar, 
                        static_cast<int> (
                            getOpts().indentSize
                        )
                    )
                );
			
            // TODO: Clear Paranthesis.
			impl->preindentString = L"";
			impl->lastWord = L""; // last TK_WORD seen
			impl->lastType = L"TK_START_EXPR"; // last token type
			impl->lastText = L""; // last token text
			impl->lastLastText = L""; // pre-last token text
			impl->input = L"";
			impl->output = std::vector<String>(); // formatted javascript gets built here
			impl->whitespace = std::vector<wchar_t> {L'\n', L'\r', L'\t', L' '};
			impl->wordchar = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$";
			impl->digits = L"0123456789";
			impl->punct = StringHelper::split(L"+ - * / % & ++ -- = += -= *= /= %= == === != !== > < >= <= >> << >>> >>>= >>= <<= && &= | || ! !! , : ? ^ ^= |= :: <?= <? ?> <%= <% %>", L' ');

			// Words which always should start on a new line
			impl->lineStarters = (StringHelper::split(L"continue,try,throw,return,var,if,switch,case,default,for,while,break,function", L','));

            // TODO: Impl setMode
            // ---

			// impl->mode(L"BLOCK");
            String mode {L"BLOCK"};

            auto prev = std::make_unique<BeautifierFlags>(L"BLOCK");

			if (getFlags().previousMode != L"BLOCK")
			{
				getFlagStore().push_back(getFlags());
                prev.reset();
                prev = std::make_unique<BeautifierFlags>(getFlags());
			}

			// BeautifierFlags tempVar {mode};
			impl->flags = tempVar;

			if (getFlagStore().size() == 1)
			{
				impl->flags.indentationLevel = 0;
			}
			else
			{
				impl->flags.indentationLevel = prev->indentationLevel;

				if (prev->varLine && prev->varLineReindented)
				{
					impl->flags.indentationLevel = getFlags().indentationLevel + 1;
				}
			}

			getFlags().previousMode = prev->mode;
            // ---
			impl->parserPos = 0;
        }
        
        /**
         * @brief Get the Opts object
         * 
         * @return BeautifierOptions 
         */
		BeautifierOptions getOpts() const
		{
			return impl->opts;
		}

        /**
         * @brief Set the Opts object
         * 
         * @param value 
         */
		void setOpts(BeautifierOptions& value)
		{
			impl->opts = value;
		}

        /**
         * @brief Get the Flags object
         * 
         * @return BeautifierFlags 
         */
		BeautifierFlags getFlags() const
		{
			return impl->flags;
		}

        /**
         * @brief Set the Flags object
         * 
         * @param value 
         */
		void setFlags(BeautifierFlags& value)
		{
		    impl->flags = value;
		}

        /**
         * @brief Get the Flag Store object
         * 
         * @return std::vector<BeautifierFlags> 
         */
		std::vector<BeautifierFlags> getFlagStore() const
		{
			return impl->flagStore;
		}

        /**
         * @brief Set the Flag Store object
         * 
         * @param value 
         */
		void setFlagStore(const std::vector<BeautifierFlags>& value)
		{
			impl->flagStore = value;
		}

        /**
         * @brief Get the Wanted Newline object
         * 
         * @return true 
         * @return false 
         */
		bool getWantedNewline() const
		{
			return impl->wantedNewline;
		}

        /**
         * @brief Set the Wanted Newline object
         * 
         * @param value 
         */
		void setWantedNewline(bool value)
		{
			impl->wantedNewline = value;
		}

        /**
         * @brief Get the Just Added Newline object
         * 
         * @return true 
         * @return false 
         */
		bool getJustAddedNewline() const
		{
			return impl->justAddedNewline;
		}

        /**
         * @brief Set the Just Added Newline object
         * 
         * @param value 
         */
		void setJustAddedNewline(bool value)
		{
			impl->justAddedNewline = value;
		}

        /**
         * @brief Get the Do Block Just Closed object
         * 
         * @return true 
         * @return false 
         */
		bool getDoBlockJustClosed() const
		{
			return impl->doBlockJustClosed;
		}

        /**
         * @brief Set the Do Block Just Closed object
         * 
         * @param value 
         */
		void setDoBlockJustClosed(bool value)
		{
			impl->doBlockJustClosed = value;
		}

        /**
         * @brief Get the Indent String object
         * 
         * @return String 
         */
		String getIndentString() const
		{
			return impl->indentString;
		}

        /**
         * @brief Set the Indent String object
         * 
         * @param value 
         */
		void setIndentString(const String& value)
		{
			impl->indentString = value;
		}

        /**
         * @brief Get the Preindent String object
         * 
         * @return String 
         */
		String getPreindentString() const
		{
			return impl->preindentString;
		}

        /**
         * @brief Set the Preindent String object
         * 
         * @param value 
         */
		void setPreindentString(const String& value)
		{
			impl->preindentString = value;
		}

        /**
         * @brief Get the Last Word object
         * 
         * @return String 
         */
		String getLastWord() const
		{
			return impl->lastWord;
		}

        /**
         * @brief Set the Last Word object
         * 
         * @param value 
         */
		void setLastWord(const String& value)
		{
			impl->lastWord = value;
		}

        /**
         * @brief Get the Last Type object
         * 
         * @return String 
         */
		String getLastType() const
		{
			return impl->lastType;
		}

        /**
         * @brief Set the Last Type object
         * 
         * @param value 
         */
		void setLastType(const String& value)
		{
			impl->lastType = value;
		}

        /**
         * @brief Get the Last Text object
         * 
         * @return String 
         */
		String getLastText() const
		{
			return impl->lastText;
		}

        /**
         * @brief Set the Last Text object
         * 
         * @param value 
         */
		void setLastText(const String& value)
		{
			impl->lastText = value;
		}

        /**
         * @brief Get the Last Last Text object
         * 
         * @return String 
         */
		String getLastLastText() const
		{
			return impl->lastLastText;
		}

        /**
         * @brief Set the Last Last Text object
         * 
         * @param value 
         */
		void setLastLastText(const String& value)
		{
			impl->lastLastText = value;
		}

        /**
         * @brief Get the Input object
         * 
         * @return String 
         */
		String getInput() const
		{
			return impl->input;
		}

        /**
         * @brief Set the Input object
         * 
         * @param value 
         */
		void setInput(const String &value)
		{
			impl->input = value;
		}

        /**
         * @brief Get the Output object
         * 
         * @return std::vector<String> 
         */
		std::vector<String> getOutput() const
		{
			return impl->output;
		}

        /**
         * @brief Set the Output object
         * 
         * @param value 
         */
		void setOutput(const std::vector<String>& value)
		{
			impl->output = value;
		}

        /**
         * @brief Get the Whitespace object
         * 
         * @return std::vector<wchar_t> 
         */
		std::vector<wchar_t> getWhitespace() const
		{
			return impl->whitespace;
		}

        /**
         * @brief Set the Whitespace object
         * 
         * @param value 
         */
		void setWhitespace(const std::vector<wchar_t>& value)
		{
			impl->whitespace = value;
		}

        /**
         * @brief Get the Wordchar object
         * 
         * @return String 
         */
		String getWordchar() const
		{
			return impl->wordchar;
		}

        /**
         * @brief Set the Wordchar object
         * 
         * @param value 
         */
		void setWordchar(const String &value)
		{
			impl->wordchar = value;
		}

        /**
         * @brief Get the Digits object
         * 
         * @return String 
         */
		String getDigits() const
		{
			return impl->digits;
		}

        /**
         * @brief Set the Digits object
         * 
         * @param value 
         */
		void setDigits(const String& value)
		{
			impl->digits = value;
		}

        /**
         * @brief Get the Punct object
         * 
         * @return std::vector<String> 
         */
		std::vector<String> getPunct() const
		{
			return impl->punct;
		}

        /**
         * @brief Set the Punct object
         * 
         * @param value 
         */
		void setPunct(const std::vector<String>& value)
		{
			impl->punct = value;
		}

        /**
         * @brief Get the Line Starters object
         * 
         * @return std::vector<String> 
         */
		std::vector<String> getLineStarters() const
		{
			return impl->lineStarters;
		}

        /**
         * @brief Set the Line Starters object
         * 
         * @param value 
         */
		void setLineStarters(const std::vector<String> &value)
		{
			impl->lineStarters = value;
		}   
        
        /**
         * @brief Get the Parser Pos object
         * 
         * @return int 
         */
		int getParserPos() const
		{
			return impl->parserPos;
		}

        /**
         * @brief Set the Parser Pos object
         * 
         * @param value 
         */
		void setParserPos(int value)
		{
			impl->parserPos = value;
		}

        /**
         * @brief 
         * 
         * @return int 
         */
		int getNNewlines() const
		{
			return impl->nNewlines;
		}

        /**
         * @brief 
         * 
         * @param value 
         */
		void setNNewlines(int value)
		{
			impl->nNewlines = value;
		}

        String beautify(String& s, BeautifierOptions opts = {})
        {
            // TODO : OPTS conditional
            // TODO : Apply Blank

            while (s.length() != 0 && (s[0] == L' ' || s[0] == L'\t'))
			{
				setPreindentString(getPreindentString() + StringHelper::toString(s[0]));
				s.erase(0, 1);
			}

            this->setInput(s);
            this->setParserPos(0);

            while (true)
            {
                auto token = getNextToken();
                auto tokenText = std::get<0>(token);
				auto tokenType = std::get<1>(token);

				if (tokenType == L"TK_EOF")
					break;


                auto handlers = std::unordered_map<String, std::function<void(const String&)>>
				{
					// {L"TK_START_EXPR", &Beautifier::HandleStartExpr},
					// {L"TK_END_EXPR", &Beautifier::HandleEndExpr},
					// {L"TK_START_BLOCK", &Beautifier::HandleStartBlock},
					// {L"TK_END_BLOCK", &Beautifier::HandleEndBlock},
					// {L"TK_WORD", &Beautifier::HandleWord},
					// {L"TK_SEMICOLON", &Beautifier::HandleSemicolon},
					// {L"TK_STRING", &Beautifier::HandleString},
					// {L"TK_EQUALS", &Beautifier::HandleEquals},
					// {L"TK_OPERATOR", &Beautifier::HandleOperator},
					// {L"TK_COMMA", &Beautifier::HandleComma},
					// {L"TK_BLOCK_COMMENT", &Beautifier::HandleBlockComment},
					// {L"TK_INLINE_COMMENT", &Beautifier::HandleInlineComment},
					// {L"TK_COMMENT", &Beautifier::HandleComment},
					// {L"TK_DOT", &Beautifier::HandleDot},
					// {L"TK_UNKNOWN", &Beautifier::HandleUnknown}
				};

            }
            
            
        }

        std::tuple<std::wstring, std::wstring> getNextToken();

        void HandleStartExpr(const String &tokenText);
        void HandleEndExpr(const String &tokenText);
        void HandleStartBlock(const String &tokenText);
        void HandleEndBlock(const String &tokenText);
        void HandleWord(const String &tokenText);
        void HandleSemicolon(const String &tokenText);
        void HandleString(const String &tokenText);
        void HandleEquals(const String &tokenText);
        void HandleOperator(const String &tokenText);
        void HandleComma(const String &tokenText);
        void HandleBlockComment(const String &tokenText);
        void HandleInlineComment(const String &tokenText);
        void HandleComment(const String &tokenText);
        void HandleDot(const String &tokenText);
        void HandleUnknown(const String &tokenText);


    private:
        struct Impl 
        {
            BeautifierOptions               opts;
            BeautifierFlags                 flags;
            std::vector<BeautifierFlags>    flagStore;
            bool                            wantedNewline {false};
            bool                            justAddedNewline {false};
            bool                            doBlockJustClosed {false};
            String                          indentString;
            String                          preindentString;
            String                          lastWord;
            String                          lastType;
            String                          lastText;
            String                          lastLastText;
            String                          input;
            std::vector<String>             output;
            std::vector<wchar_t>            whitespace;
            String                          wordchar;
            String                          digits;
            std::vector<String>             punct;
            std::vector<String>             lineStarters;
            int                             parserPos = 0;
            int                             nNewlines = 0;

            // Impl() = default;
        };
        std::unique_ptr<Impl> impl;
    };
}

#endif /* End of include guard : JS_BEAUTIFIER_HPP */