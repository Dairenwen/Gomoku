#pragma once
#include "log.hpp"
#include <vector>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
class stringtool;
class stringtool
{
public:
    static bool split(const std::string& str,const std::string& gap,std::vector<std::string>& ret)
    {
        ret.clear();
        int n=str.size(),cur=0,pos=0;
        while( cur<n && (pos=str.find(gap,cur))!=std::string::npos)
        {
            std::string pushstr=str.substr(cur,pos-cur);
            if(!pushstr.empty()) ret.push_back(pushstr);//防止字符串为空
            cur=pos+1;
        }
        if(cur<n) ret.push_back(str.substr(cur));//添加最后一个子字符串
        LOG(DEB,"string split successfully");
        return true;
    }

    static std::vector<int> computePrefixFunction(const std::string& pattern) {
        int m = pattern.size();
        std::vector<int> pi(m, 0);
        for (int i = 1; i < m; ++i) {
            int j = pi[i - 1];  // 上一个位置的最长前缀长度
            // 不匹配时回溯到更短的前缀
            while (j > 0 && pattern[i] != pattern[j]) {
                j = pi[j - 1];
            }
            // 匹配则延长前缀长度
            if (pattern[i] == pattern[j]) {
                j++;
            }
            pi[i] = j;
        }
        return pi;
    }

    /**
     * KMP匹配算法：在文本中查找所有模式串（敏感词）的起始位置
     * @param text 待匹配文本
     * @param pattern 模式串（敏感词）
     * @param pi 前缀函数数组（由computePrefixFunction生成）
     * @return 所有匹配的起始索引列表
     */
    static std::vector<int> kmpSearch(const std::string& text, const std::string& pattern, const std::vector<int>& pi) {
        std::vector<int> matches;
        int n = text.size();
        int m = pattern.size();
        if (m == 0 || n < m) return matches;  // 模式串为空或文本过短，直接返回

        int j = 0;  // 当前匹配的模式串长度
        for (int i = 0; i < n; ++i) {
            // 不匹配时通过前缀函数回溯，避免重复比较
            while (j > 0 && text[i] != pattern[j]) {
                j = pi[j - 1];
            }
            // 匹配则推进模式串指针
            if (text[i] == pattern[j]) {
                j++;
            }
            // 找到完整匹配，记录起始位置
            if (j == m) {
                matches.push_back(i - m + 1);  // 起始索引 = 当前位置 - 模式串长度 + 1
                j = pi[j - 1];  // 继续查找下一个匹配
            }
        }
        return matches;
    }

    /**
     * 基于KMP算法的敏感词过滤函数（最长敏感词优先匹配）
     * @param input 待过滤的原始字符串
     * @return 过滤后的字符串（敏感词替换为同等长度的'*'）
     */
    static std::string filterSensitiveWords(const std::string& input) {
        // 敏感词集合（示例，实际使用需替换为真实敏感词）
        std::unordered_set<std::string> sensitiveSet = {
            "病","妈","操","草","卧槽","去世","傻逼","垃圾", "混蛋", "笨蛋", "愚蠢", "去你妈的",
            "fuck","shit","rubbish","ass"
        };

        // 边界情况：无敏感词或空输入直接返回
        if (sensitiveSet.empty() || input.empty()) {
            return input;
        }

        // 敏感词转为向量并按长度降序排序（确保最长词优先匹配）
        std::vector<std::string> sensitiveWords(sensitiveSet.begin(), sensitiveSet.end());
        std::sort(sensitiveWords.begin(), sensitiveWords.end(),
            [](const std::string& a, const std::string& b) {
                return a.size() > b.size();  // 长词在前
            });

        int textLen = input.size();
        std::vector<bool> needReplace(textLen, false);  // 标记是否需要替换

        // 遍历每个敏感词，用KMP查找并标记替换位置
        for (const std::string& word : sensitiveWords) {
            int wordLen = word.size();
            if (wordLen == 0) continue;  // 跳过空字符串

            // 预处理前缀函数
            std::vector<int> pi = computePrefixFunction(word);
            // 查找所有匹配位置
            std::vector<int> matches = kmpSearch(input, word, pi);

            // 标记匹配区间（已被标记的位置不再重复处理）
            for (int start : matches) {
                int end = start + wordLen;
                if (end > textLen) continue;  // 越界检查

                // 检查该区间是否未被更长的敏感词覆盖
                bool canReplace = true;
                for (int i = start; i < end; ++i) {
                    if (needReplace[i]) {
                        canReplace = false;
                        break;
                    }
                }

                // 标记需要替换的位置
                if (canReplace) {
                    for (int i = start; i < end; ++i) {
                        needReplace[i] = true;
                    }
                }
            }
        }

        // 生成过滤后的结果
        std::string result;
        for (int i = 0; i < textLen; ++i) {
            result += (needReplace[i] ? '*' : input[i]);
        }

        return result;
    }
};