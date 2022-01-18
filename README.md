# ACID: 高性能协程RPC网络框架

> 学习本项目需要有一定的C++，网络，RPC知识

本项目将从零开始搭建出一个基于协程的异步RPC框架。

通过本项目你将学习到：
- 序列化协议
- 通信协议
- 服务注册
- 服务发现
- 负载均衡
- 心跳机制
- 三种异步调用方式


相信大家对RPC的相关概念都已经很熟悉了，这里不做过多介绍，直接进入重点。
本文档将简单介绍框架的设计，在最后的 examples 会给出完整的使用范例。
更多的细节需要仔细阅读源码。

## RPC框架设计
本RPC框架主要有网络模块， 序列化模块，通信协议模块，客户端模块，服务端模块，服务注册中心模块，负载均衡模块

主要有以下三个角色：

![role](data:image/jpg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAsICAoIBwsKCQoNDAsNERwSEQ8PESIZGhQcKSQrKigkJyctMkA3LTA9MCcnOEw5PUNFSElIKzZPVU5GVEBHSEX/2wBDAQwNDREPESESEiFFLicuRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUVFRUX/wAARCAHMAoADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwD1qiiimMKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAqK4uY7ZVaTd8x2gKpJJqaqWof6y0/66/wDsppxV2S3ZC/2jF/zzn/78tR/aMX/POf8A79NSUU7IV2L/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Y6iq+r6QbvQU023UMFMaBnP3FB5b3OP50Jwe6G4y6FrSb2TULEXUiBFkZjGB12Z4J9z1q9TIokgiSKMYRFCqPQCn1i2r6GiWgUUUUhhVe0+5L/11f+dWKr2n3Jf+ur/zqlsJ7oJP+P23/wB1/wCQp11/x6zf7jfypkn/AB+2/wDuv/IU+6/49Jv+ubfyproT3LsH/HvH/uD+VSVHB/x7x/7g/lUlZM0WwlFLRQMKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKAK2o/8g+4/wBw0yn6j/yD7j/cNMqlsQ9xaKKKQBRRRTAKKKSgAoopaYBRRRSGFFFFABRRRQAVXtPuSf8AXV/51YqvZ/6uX/rs/wDOqWwnuMnLi8tvLjMjYf5QQOw9adMLuSCRBZsCykDMi9x9ak6ajbf7sn8hWhScrWEle42JSkKKeqqAfyp1FFQWFFFFAwooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAraj/yDrj/AHDTO1TXkTT2ksaY3OpAzVXbe/8APtH/AN/f/rVS2Ie5JRUe29/59o/+/v8A9ajbe/8APtH/AN/f/rUWFcloqLbe/wDPtH/39/8ArUbbz/n2j/7+/wD1qLBckoqPbef8+0f/AH9/+tRtvP8An2j/AO/v/wBanYLklFR7b3/n2j/7+/8A1qNt7/z7R/8Af3/61FguS0VFtvf+faP/AL+//Wo23v8Az7R/9/f/AK1Fh3JaKi23v/PtH/39/wDrUbb3/n2j/wC/v/1qVguS0VFtvf8An2j/AO/v/wBajbe/8+0f/f3/AOtRYLktVrOyjmSV2aUEyvwshA6+gqTbe/8APtH/AN/f/rVYsYZIYGEoCuzs2Ac4yad7LcW7EhsIYZhKDIzqCAXkLYz161ZooqG7lpWCiiigYUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABS1Bd3DWtu0qwvMV/gTrWGfF0f8No/wCLisp1oQdpM1hRnUV4q50dJXMN4uf+G0UfV/8A61ami6q+qRytJGqGNgMKc5GKmFenN8sWOeHqQjzSRp0UUVuYhRRRQAUUUUAFFFFABRRRQAUUUUAU73VbXT3VLlyrOMjCk8VW/wCEk03/AJ7N/wB+zWT4t/4/Lf8A65n+dc/XnVcTOE3FHpUcJCpTUm2dsfEmmgczN/37NaUUqzRLImdrjI3Ag/ka5Lw1HZS3RFzzOOY1b7p/+vXY100JzqR5pWOTEU4U5csbiUUUV0GAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFLQAVwuvS2kuosbRAAOHYdGb2rZ8Q6yYQ1nbN+8IxIw/hHp9a5TFebi6yfuI9PBUWv3jErpvCLfNdL7Kf51k2OjXWoQSTQgBV4G7jefQVqeF0eG/uYpFKOEGVYcjmsqEZRqRbW5tiZxlTkk9UdRRRRXrnihRRRQMKKKKACiiigAooooAKKKKAOU8WjN7b/wDXM/zrGurK4syguIim8blz3ruZtOiuNQhupfm8pcKuOM561Je2UN/btDMuQeh7g+orhqYZzcpHfSxfs4xjbTqedqSrBlJBHII7V1+ha4LwC2umAuB91v7/AP8AXrm9R06bTbjy5RlT9xx0YVVVirAqSCOQR2rjp1JUZHbUpQrwuj0qisTQ9cF4ot7kgXAHyt/f/wDr1t168Jxmro8acJQlyyCiiirICiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKWgArH13WBp8PlQkG5kHH+yPWrWq6lHplqZGw0jcInqf8K4WaWS4meWZizuckmuPE1+RcsdzswuH9o+aWwwsWJZiSScknvV7SdMfU7rYMrEvMj+g9PrUFlZy39ysEI5PU9lHqa7uxsorC1WCEcDqT1Y+prlw9H2j5nsdmJr+zXLHcmhhSCJYolCogwAO1HlR+d5uxfMxt3Y5x6U6ivVsjx7hRRRTAKKKKACiiigAooooAKKKKACiiigAopKKAIbyzhvrdoZlyp6HuD6iuH1LTptNuPLkGVP3HHRhXf1BeWcN9btDMuVPQ9wfUVzV6CqK63OnD4h0n5HnikqwKkgjkEdq6/Q9cF4BbXRAuAPlb+//wDXrm9S02bTbgxycqfuOOjCqgJUggkEcgjtXnU6kqMj06lOFeF0elUViaHrgvFFtcsBcD7rf3//AK9bdevCamrxPGnCVOXLIKKKKsgKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKAFqC8u4rG2eeY4Ve3cn0FSSypBG0kjBUUZJPauH1fVH1O5yMrCn3F/qfeuevWVOPmb4eg6svIr317LqF008p68KvZR6VFFE88qxRKWkc4AFM5JAAyTwAK7LQdH+wRedOP9Icf98D0rzqVOVaWp6tarGhCy+RZ0nS00y228NK/Lv6n0+laFFJXrxioqyPElJyd2FFFFUIKKKKACiiigAooooAKKKKACiiigAooooAKSlpKAClpKWgCG7s4b63aGdcqe/cH1FcNqWmy6bceXJyh+446MP8AGu/qC8s4b63aGZcqeh7g+ormr0FUV1udOHxDpPyPPFYqQVJBHII7V1+ha4LwC2uiBcAfK39//wCvXN6lps2m3HlycofuOOjD/GqisVIKkgjkEdq86E5UZHp1KcK8Lo9KorD0PXBeAW10QLgD5W/v/wD163K9eE4zXNE8acJU5csgoooqyAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiigkKCSQAO5oAKKajpIu6NlYeqnNOoEFFFJkEkZGR2oGLRRRQIKKb5ibtu9d3pkZp2QDjPNABRRRQMKKKKACiiigAooooAKCcClrmPEWs53WVs3tK4/9BH9azqVFTjdmlKnKpLlRU17WPt0pt4G/wBHQ8kfxn/C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FFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFACUtFFACUUtJQAUtJRQBFd2cN9btDOu5T+YPqK4bUtNm0242SfMh+446MP8a7+obyzivrdoZ1yp79wfUVzV6CqK63OnD4h0n5HnYYqQVJBHII7V12h66LwLbXRAuB91v7/wD9euc1LTZtNuPLkGUP3HHRh/jVMEgggkEcgjtXnQnKjI9OpThXhdHpdFYeha4LtRbXTAXA+6x/j/8Ar1uV68JqaujxpwlTlyyCiiirICiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiikoAKWiigAqpqWoRadamWTluiJ3Y1NdXMdnbvNM21EGT/AIVwmo6hJqN000nA6In90VzV6ypqy3OnD0HVlrsQ3NxLd3DzTNudz+XtRa2st5cJBCMu35AeppiRvLIscalnY4AHc12+jaSumW/zYad/vt6ew9q4KNJ1ZXex6VetGjGy3LGnWEenWiwx8nqzd2PrVqiivXSSVkeK227sKKSlpiCiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooASilpKAClpKWgCC8s4b63aGdcqeh7g+orh9S02bTZ9kgyh+446MP8a7+obu0ivrdoZ13KfzB9RXNXoKorrc6cPiHSfkedgkEEEgjkEdq67Q9cF2BbXRxOPut/f8A/r1zmpabNptxskGUP3HHRh/jVRSQQQcEcgjtXnQnKjM9OpThXhdHpVFYeh64LsC2umAnH3WP8f8A9etyvXhNTXMjxqkJU5csgoooqyAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACikpaACkd1jRnchVUZJPal6VyPiDWftbm1t2/cKfnYfxn/CsatVU43ZrRpOrLlRU1rV21K42oSLdD8i/wB73NZtFdH4d0beVvblflHMSHv/ALR/pXlRjOvM9iUoYenoXPD+jfY0F1cL+/cfKp/gH+NblFFexCChHlR4s5upLmkFFFFWQJS0UUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAJRRRQAUtJRQBFeWcV9btDOuVbv3B9RXDalps2mT7JOUP3HHRh/jXf1Dd2cV9btDOuVP5g+ormr0FUV1udOHxDpPyPOwSCCCQRyCO1ddoeuC7AtrpgJx91j/H/APXrnNS02bTbjy5BlD9xx0Yf41UBIIIJBHII7V50JyoyPTqU4V4XR6VRWHoeuC7C210wE4+639//AOvW5XrwmprmieNOEqcuWQUUUVZAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFAHO+IdZEatZWzfOeJGH8I9PrXLV02vaEcvd2i5zzJGP5isjSdLfU7rZysK8yP6D0+teTXjUnUs/kexh5UoUuZfMs6Do5v5vPnX/AEZD0/vn0+ldmAFAAGAOgFNhiSCJYolCoowAO1Or0KNJU42PMrVnVldhRRRWxkFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAJRRRQAtFJS0AQ3lnDfW7QzrlT0PcH1FcPqWmy6bPskGUP3H7MP8a76obu0ivrdoZ1yp/MH1Fc1egqiutzow+IdJ+R52GIIIJBHII7V12h64LsLbXTATj7rH+P/AOvXO6lps2m3GyT5kP3H7MP8a2NB0IkpeXa4A+aOM/zNcWHVSNSy+Z34l0p0+Zv0Omooor1jyAooooAKSiigAooooAKKKKAFooooAKKKKACiiigAooooAKKKSgAooooAKWkooAWo4YIrdWWGNUDMWIUYyTUlFKwBRRRTAKKKKACiikoAKWkpaACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiikoAKKWigApKNw9R+dG4eo/OgVxaKTI9R+dG4eo/OgLjZoIrhAsyK6ghgGHcU+k3D1H50bh6j86Vh3FopNw9R+dGR6j86Yri0Um4eo/OjcPUfnQAtFJkeo/OjI9R+dAC0Um4eo/OjcPUfnQAtJRuHqPzo3D1H50BcKKKKBi0UlLQAUUUUAFFFFABRRRQAUUUUAFFFFAgooooAKKKKACiiigYUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFJRQAUUUtACUUUUALSUUUAFFFFAC0UlFAC0UUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAJS0UUAFRXRItZiDghG/lTLq6Nu8aLE0jyZwAQOn1qtNdTywyRizYF1K5Mi9xVKL3IclsQw2VsYIibeMkoCSV9qk+w2v/PvF/wB8imRy3KRIhtCdqgf6wdhTvPuP+fQ/9/BWmpKsL9htf+feL/vkUfYbX/n3i/75FJ59x/z6H/v4KPPuP+fQ/wDfwUtQ0F+w2v8Az7xf98ij7Da/8+8X/fIpPPuP+fQ/9/BR59x/z6H/AL+CnqGgv2G1/wCfeL/vkUfYbX/n3i/75FJ59x/z6H/v4KPPuP8An0P/AH8FGoaC/YbX/n3i/wC+RR9htf8An3i/75FJ59x/z6H/AL+Cjz7j/n0P/fwUtQ0F+w2v/PvF/wB8ij7Da/8APvF/3yKTz7j/AJ9D/wB/BR59x/z6H/v4KNQ0F+w2v/PvF/3yKPsNr/z7xf8AfIpPPuP+fQ/9/BR59x/z6H/v4KNQ0F+w2v8Az7xf98ij7Da/8+8X/fIpkl1NFG0klqQi8kiQVbod0NWYzTEVEuFQAKJiAB0HAq7VTTv+Xn/ru38hVyoluVHYSlpKWpKCiiigDF8TX9xp9lE9rJ5bNJtJwDxg+tcx/wAJJqv/AD9n/vhf8K3/ABl/yD7f/rr/AENcZXo4aEXTu0ebiJyVSyZq/wDCSar/AM/Z/wC+F/wpP+Ek1X/n7P8A3wv+FZdFdHsodjD2k+5qf8JJqv8Az9n/AL4X/Cj/AISTVf8An7P/AHwv+FZdFHsqfYPaT7mp/wAJJqv/AD9n/vhf8KP+Ek1X/n7P/fC/4Vl0Ueyh2H7Sfc1P+Ek1X/n7P/fC/wCFH/CSar/z9n/vhf8ACsuij2UOwvaT7mp/wkmq/wDP2f8Avhf8KP8AhJNV/wCfs/8AfC/4Vl0Ueyh2H7Sfc1P+Ek1X/n7P/fC/4Uf8JJqv/P2f++F/wrLoo9lDsL2k+5qf8JJqv/P2f++F/wAKP+Ek1X/n7P8A3wv+FZdFHsodg9pPuav/AAkmq/8AP2f++F/wrvYmLQRsxySoJP4V5b616jB/x7xf7g/lXFioxjayOzCybvdklJS0VxnaFFFJQAtFJRQAtFJRQAtJRS0AJRS0UAJRS0UAJRS0UAJRS0UAJRS0UAJS0UhIBAJAJ6UALRRRQAlLRRQAUUUlAC0UlFAC0UlLQAUUUUAFFFFABTDPEpIaRAR2LCn1kw28LtMzxIzGZ+SoJ600rkttbEt5NEby1IlQgB8ncPQU7z4v+eqf99Cmi1g/54x/98Cl+zQf88Y/++BWmlidR3nxf89U/wC+hR58X/PVP++hTfs0H/PGP/vgUfZoP+eEf/fAo0DUXzov+eqf99Cjzov+eqf99Ck+zQf88Y/++BR9mg/54x/98CjQNRfOi/56p/30KPOi/wCeqf8AfQpPs0H/ADwj/wC+BR9mg/54R/8AfAo0HqL50X/PVP8AvoUedF/z1T/voUn2aD/nhH/3wKPs0H/PCP8A74FGgtRfOi/56p/30KPOi/56p/30KT7NB/zwj/74FH2aD/njH/3wKNA1F86L/nqn/fQo86L/AJ6p/wB9Ck+zQf8APGP/AL4FH2aD/njH/wB8CjQNRfOi/wCeqf8AfQpyuj/cdWx6HNM+zQf88Y/++BUSRpHqQ8tFTMJztGM/NRZBdjr/AP5B8/8AuGrFV9Q/5B9x/uGrBpdAW4mnf8vP/Xc/yFXO9U9P/wCXn/rsf5CrlRLcqOwUUUUigoopaAOc8Zf8g+D/AK6/0NcZXZ+Mv+QfB/11/oa4yvTwv8M8vE/xAooorpOcKSlpVG5gMgZOMntQA2itTVNPtrBjCjXLXAI++gCOPUGq02lXsCo0kDKHIUcjgnoD6H61CnFq5bg07FSirs+kX1tC0s1uURcbskZGfaki0u8mZVSAksgk5IACnoSe1Pnja9xcsr2sU6KuSafJFbFmR/NEvlnGCvTOOO9MutOurJVa4iKKxwDkHn0OOhoU4vqHK+xWoooqiRaKSigBexr1GD/j2i/3B/KvLvWvUYP+PaL/AHB/KuHGdDuwnUkooorhO4SilooASilpKACiiigApaKKACvPg5fWWji8+K9k1Ngl00hEewHJTrgnHbHevQay5PD9lNaT27+YVmnNwW3fMjnup7VcJKN7mc4uVrGVf+KbmDVbmC3gWSO1dUZBE7PJnBOCOFxnv1p2oa9qdvLqUkENqbawdVbfu3vkDp271pyaDC921zFdXdvLIFEphl2iXAwCwx1p82h208V9G7SYvWDSEHkYxjH5VSlBdCXGfcyX8RX1gL8X8EDSQQxzRrCTj5ztCkn370+61XVLRrizuTb/AGl7R7iGWEHCleoIP6GtS40SzupLh51Z/tEKwOpPG0HIx7+9R2ug21u8sjy3FzJJF5O+eTcVT0HpRzQ7Byy7mQ/iC/s9M0xZmhe6vE3+aIncKm0HlV5Lc9uK3dHvpNS0yK5mhMMjZDIQRyDjIzzg9arDw5bCzggFxdA27EwyiTDxjGMA46e1aVrb/ZbdYjNLNt/jlbcx+pqZOLWhUVK+pymn6pfW9nZW9msUs13dzpunLYUKSe1W7fxHdL9ma9SFIjdyWk8i5AVh90jPQHpWlBoVrbvbNGZM20skqZbqX65pJvD9nPp91ZSeYYrmUzMd3zBic8HtVuUG9iVGS6mdFr19eyW0NpHAj3bytE8oJCxIQASB1JNUNS17b9hu76MK9heyRzCLkMRGeRn1yK6K50O1uIbVEaW3a0GIZIW2sgxgjPoaZH4dsY47ZNruIJGly7bjIzDBLZ69aFKC6A4zM++1+9s7Ow8xIVub1mYFUaRYkAz0Xlj06VraLfS6lpkVxcQmGUkqy7SOQcZAPOD1qsPDVmlrHbxSXEYhkMkLpJhoSeoU46e1aVrbm1gERmmnwSd8zbmP41EnG2hUVK+pNRRSVBoFFFFABRS0UAJRS0UAJRS0UAJS0UUAFZtv/wAtv+uz/wA60qzbfpL/ANdn/nVR2ZD3RNS0lLTGFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFV/+Ykv/AFxP/oQqxVb/AJiS/wDXE/8AoQpksXUP+PC4/wBw1YqvqH/Hhcf7hqxR0GtxNP8A+Xn/AK7H+Qq5VPT/APl5/wCu5/kKuVEtxx2CiiikUFLSUtAHOeMv+QfB/wBdf6GuMrs/GX/IOg/66/0NcZXp4X+GeXif4gUUUV0nOJTkwXAY7VJ5OM4FWbCxa+ldd6xxxqXkkboqii5t7RVU2l287lsbGhKn6ip5lflK5Xa5qPf28GnCCS7F8yyI0ICEGMA5PJ9emKff6pbzsXhuYxFJMkjRCAq/B7t3xWLNY3duUE9vLGX4XcvWllsLyCISS20qITjcykc1kqVPR3NfaTtaxpQ6jbHWrxrhi1ndFlY4PTqDSnUre9F/DPIbdJypifaSFC9AQO2KzJLC7hMYltpEMhwgZcbj6Co/s821m8psI2xjjo3p9ar2cHqmTzyWjRq297bWVtFGknmmK7EvCkblA680usX8dzCyw3UTxtLv8tYCjfUnvVA6VqAxmznGTgfIetRrYXbXDW628hmXqgXkUuSm5c1x80+XlsV6KsR6fdyyPHHbSs8Zw6heV+tQMrIxVgQwOCD2NbXT2MrNbiUUUUCF7GvUbf8A49ov9wfyry7sa9Rt/wDj2i/3F/lXDjOh3YTqSUUVi6zrkumXSRRwo4ZN2WJ9a86c1BXkejCnKpLljubVFcp/wltx/wA+0X/fRo/4S24/59ov++jWP1ql3N/qlbsdXRXKf8Jbcf8APtF/30aP+EtuP+faL/vo0vrVLuL6pW7HV0YrlP8AhLbj/n2i/wC+jR/wltx/z7Rf99Gj61S7h9Urdjq6K5T/AIS24/59ov8Avo0f8Jbcf8+0X/fRo+tUu4fVK3Y6uiuU/wCEtuP+faL/AL6NH/CW3H/PtF/30aPrVLuH1St2Orpa5P8A4S24/wCfaL/vo0f8Jbcf8+0X/fRo+tUu4fVK3Y6ykrlP+EtuP+faL/vo0f8ACW3H/PtF/wB9Gj61S7h9Urdjq6K5T/hLbj/n2i/76NH/AAltx/z7Rf8AfRo+tUu4fVK3Y6uiuU/4S24/59ov++jR/wAJbcf8+0X/AH0aPrVLuH1St2Orpa5P/hLbj/n2i/76NH/CW3H/AD7Rf99Gj61S7h9Urdjq6K5T/hLbj/n2i/76NH/CW3H/AD7Rf99Gj61S7h9Urdjq6K5T/hLbj/n2i/76NH/CW3H/AD7Rf99Gj61S7h9Urdjq8UYrlP8AhLbj/n2i/wC+jR/wltx/z7Rf99Gn9apdw+qVux1dFcp/wltx/wA+0X/fRo/4S24/59ov++jS+tUu4fVK3Y6uiuU/4S24/wCfaL/vo0f8Jbcf8+0X/fRo+tUu4fVK3Y6tuFJ9BWbpGsR6nEVbCTr95PX3FYx8WXBBH2aLkf3jWFDLJBKskTlXU5DCsqmLSknHbqb08HJxano+h6RRWbo+sR6lFtbCXCj5k9fcVpV2Qmpq6OGUXB8sgrNtukv/AF2f+daVZtt0l/67P/OtY7Mye6JqWs3Xftp0qX+z9/nZXOw4bbn5tvvis+HW7Gw0Z7qC4uLk+YI/KuXw0bnsSRwO9WotrQlySdmdDS1zy+KQ+nvPHbCWZLhbfy45QVYt0IbHSpjr720d8L+1EE9rGsgjWTdvDcDBx64FHJIOeJtUtYDa2LKTU5blJSYBCBFvyNzr91eOOetSXmt3Wm2ayX9nFDLLIEiXzwVORkljjgCnyMOdG3SVQ0bVF1azMwQIySGNwrbhkdwe4rnJtQvGvrpLW9unvFvNkduoLJ5eec8YA696FBttA5pWOzorB/4SOURXlw1gVtbZ2j3+bku4OAAMdyRSy69dWomju9PCXCQfaFRJdwZAfm5xwRS9nIOeJu0VgaproFvdrbq7JFarO00b7Su4jaBwecc0zVtXuXtdShsbXeltDiWcy7SrFc8Dvimqcg50dFRXM3HihNNSGDy1laO3SSQvLtJyBwuep71oDWJp9U+yWlmJUEccrzNJtCq3tjmk4SQc6NWoP+Ykv/XE/wDoQqeq/wDzEl/64n/0IUhsXUP+Qfcf7hqxVfUP+QfP/uGrFHQa3E0//l5/67n+Qq5VPT/+Xr/ruf5CrlTLccdgoooqSgpaSigDnfGX/IPg/wCuv9DXGV2fjL/kHwf9df6GuMr08L/DPLxP8QKKKK6DnNLSLiCJrmC5fy47mIx+ZjIU9QT7VLaxW+l31vcyXlvOqyDKwksQPX8KyKKhwu3ruWp6LTY3WnhtrVonvI7hpbpJRsYsEUHkn0zS3GoxzNqgNyD5kqGEkk8Buo+lYNFL2K6le2Z0F1c2n2mzuXlhe5EwaVoGJUqMfMQehpty9pDaXIjvYpnmullCpnhQawaKSopW1D2z7G/d6okh1cpck+btEWGPIzzirCalaNLOglizLbxKHlzt3KOQSOa5iik6EWrDVeV7m9calmLUibiHzpREoMBIDAdcZ56Vg0UVpCChsZym5biUUtJVki9jXqNv/wAe0X+4P5V5d2Neo2//AB7Rf7g/lXDjOh24TqSVyXipWOoxEKSPK7D3NdbQcV5lWn7SPLc9SjV9lPmPNdjf3W/KjY391vyr0rA9KMVyfUv7x2fX3/KebbG/ut+VGxv7rflXpOBRgUfUV/MH19/ynm2xv7p/KjY391vyr0nAowKPqK/mD6+/5TzbY391vypNjf3W/KvSsCjHsKPqK/mD6+/5TzbY391vyo2N/db8q9JwKMD0o+or+YPr7/lPNtjf3T+Ro2N/dP5GvScCjAo+oruH19/ynm2xv7p/Kk2N/db8q9KwKMCj6iv5g+vv+U822N/db8qNjf3W/KvSMClwKPqK/mD6+/5TzXY391vypdjf3W/KvScCjAo+o/3g+vv+U822N/dP5UbG/ut+Vek4FGBR9RX8wfX3/Kea7G/un8jS7G/un8jXpOBRij6iv5g+vv8AlPNtjf3T+VGxv7rflXpOBRgUfUV/MH19/wAp5tsb+635UbG/ut+Vek4FGBR9RX8wfX3/ACnm2xv7rflRsb+635V6TgUYo+or+YPr7/lPNdjf3W/KjY391vyr0rA9KMe1H1H+8H19/wAp5tsb+635Gm16UwGxuOxrktE0I3bLc3S4gHKqer//AFqxqYVxaUdbm1PGKUW5K1h3h/R5JpUvJi0cSHKAHBc/4fzrrKAAoAAAA4AFFejSpqnGyPMq1XVlzMKzbfpL/wBdn/nWnWZb9Jf+uz/zreOzMHuhLyKeaDbbXJt5AwIcIG/DBrLPhtJYbk3N08l1cSrKZ1QLtZfu4Xp/jW3S1ak1sJxT3MptIlmgijubsSGOdJgywKn3e2B6+tLqOiQajqNrdysQYD8yAcSDOQD9DWpRQpNByrYyrrQort9QaWVv9N8s8D/VlBwR60ybRri7t1S71J5ZopBJDL5SjYR7d8+9a9FPnYuRFeygmt4Sk84nfOdwiCfhgUyxsBYvdMrlvtExlORjGe1W6KVx2Rmf2JE2nXVnJIzJczNLuAwVJORj6YotdIZLp7m9umu5TF5KlkChUPUYHc+tadFHMw5UYlv4aht9Hu9PWZz9pPzSkcgdh+AouvDzTSXX2e/lt4btQJo1QNuOMZyenFbdFPnl3FyIx30ErOk9pdmCXyVhctEsgcKODg9DVy3sPI1Ce7Mu9poo4yNuMbc8/jmrtFLmbDlSEqD/AJiS/wDXE/8AoQqxVc/8hJf+uJ/9CFIbDUP+Qfcf7hqxVbUP+Qfcf7hp3261/wCfiP8A76os7aBdJk2n/wDL1/13P8hVyqWmOsi3LIwZTMcEd+BV2pluVHYKKKKkoKKKKAOd8Zf8g+D/AK6/0NcZXZ+M/wDkHQf9df6GuLyPUV6eF/hnl4n+ILRSZHqKMj2roOcWikz7ijI9RQAtFJkeooyPWi4C0UmR60ZHqKLgLRSZHqKMj1FFwFopMj1FGR6ii4C0lGR6ijI9RRcBexr1GD/j2i/3B/KvLsjB5Feo2/8Ax7Rf7g/lXFjOh3YTqPopaSuE7gooooAWikooAKKKKACiiigAooooAWikooAWkoooAKKKKACiiigAooooAKKKKACiiigApaSigAooooAWikooAWgcdKSloAKKKKAFrMt+kv8A12f+daVZ1v0l/wCuz/zqo7Mh7olpaKKYwooooAKKKKACiiigAooooAKSlooAKKKKACq//MSX/rif/QhViqzQtNqSqsrRkQk5UDn5h600Sxb/AP48Zv8AdrS2rj7o/KqT6a0iFHu5ircEYXn9KvVLatZDS1AADoAPpRRRUlhRRRQAUUUUAMlhinXbNGkig5w6g1D/AGdZf8+kH/fsVZoouxNJ7lb+zrL/AJ9IP+/Ypf7Osv8An0g/79irFFO7FyrsV/7Osv8An0g/79ij+zrL/n0g/wC/YqxRRdhyrsV/7Osv+fSD/v2KP7Osv+fSD/v2KsUUXYcsexX/ALOsv+fSD/v2KP7Osv8An0g/79irFFF2HKuxX/s6y/59IP8Av2KP7Osv+fSD/v2KsUUXYcq7Ff8As6y/59IP+/Yo/s6y/wCfSD/v2KsUUXYcq7Ff+z7P/n0g/wC/Yo/s+z/59IP+/YqxRRdhyrsV/wCz7P8A59IP+/YqwAAMAYAoopXuNJLYKSlpKBhRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUUUUAFFFFABRRRQAUtJRQAtZEdzFC8ySMVbzXOCp6E/StejNNOxLVzM+22//AD0/8dP+FH223/56f+On/CtPNGafMuwrPuZn223/AOen/jrf4Ufbbf8A56f+Ot/hWnmjNF12Cz7mZ9tt/wDnp/463+FH223/AOen/jrf4Vp5ozRddgs+5mfbbf8A56f+Ot/hR9tt/wDnp/463+FaeaM0XXYLPuZn223/AOen/jp/wo+22/8Az0/8db/CtPNGaLrsFn3Mz7bb/wDPT/x1v8KPttv/AM9P/HW/wrTzRmi67BZ9zM+22/8Az0/8db/Cj7bb/wDPQ/8AfLf4Vp5ozRzLsFn3Mz7bb/3z/wB8t/hS2sqTanujJKiEgnBHO4VpZoo5kHKwoooqSwooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigApKWigBKKWigBKWiigBKKWigBKKWigBKKWigBKKWigBKKWigBKKWigApKWigBKKWigBKKWigBKKWigBKKWigBKKWigBKKWigBKKWigBKKWigBKWiigApKWigBKKWigBKKWigBKKWigBKKWigBKKWigBKKWigBKWiigAopKWgAooooAKKKKBH//Z)

#### 注册中心 Registry

主要是用来完成服务注册和服务发现的工作。同时需要维护服务下线机制，管理了服务器的存活状态。

#### 服务提供方 Service Provider

其需要对外提供服务接口，它需要在应用启动时连接注册中心，将服务名发往注册中心。服务端还需要启动Socket服务监听客户端请求。

#### 服务消费方 Service Consumer

客户端需要有从注册中心获取服务的基本能力，它需要在发起调用时，
从注册中心拉取开放服务的服务器地址列表存入本地缓存，
然后选择一种负载均衡策略从本地缓存中筛选出一个目标地址发起调用，
并将这个连接存入连接池等待下一次调用。

### 序列化协议
序列化有以下规则：
1. 默认情况下序列化，8，16位类型以及浮点数不压缩，32，64位有符号/无符号数采用 zigzag 和 varints 编码压缩
2. 针对 std::string 会将长度信息压缩序列化作为元数据，然后将原数据直接写入。char数组会先转换成 std::string 后按此规则序列化
3. 调用 writeFint 将不会压缩数字，调用 writeRowData 不会加入长度信息

对于任意类型，只要实现了以下的重载，即可参与传输时的序列化。
```c++
template<typename T>
Serializer &operator >> (T& i){
   return *this;
}
template<typename T>
Serializer &operator << (T i){
    return *this;
}
```
    
### 通信协议

封装通信协议，使RPC Server和RPC Client 可以基于同一协议通信。

采用私有通信协议，协议如下：

第一个字节是魔法数。

第二个字节代表协议版本号，以便对协议进行扩展，使用不同的协议解析器。

第三个字节是请求类型，如心跳包，请求，响应等。

第四个字节表示消息长度，占四个字节，表示后面还有多少个字节的数据。

除了协议头固定7字节，消息不定长。

目前提供了以下几种请求
```c++
enum class MsgType : uint8_t {
    HEARTBEAT_PACKET,       // 心跳包
    RPC_PROVIDER,           // 向服务中心声明为provider
    RPC_CONSUMER,           // 向服务中心声明为consumer
    
    RPC_REQUEST,            // 通用请求
    RPC_RESPONSE,           // 通用响应
    
    RPC_METHOD_REQUEST ,    // 请求方法调用
    RPC_METHOD_RESPONSE,    // 响应方法调用
    
    RPC_SERVICE_REGISTER,   // 向中心注册服务
    RPC_SERVICE_REGISTER_RESPONSE,
    
    RPC_SERVICE_DISCOVER,   // 向中心请求服务发现
    RPC_SERVICE_DISCOVER_RESPONSE
};
```
### 服务注册
每一个服务提供者对应的机器或者实例在启动运行的时候，
都去向注册中心注册自己提供的服务以及开放的端口。
注册中心维护一个服务名到服务地址的多重映射，一个服务下有多个服务地址，
同时需要维护连接状态，断开连接后移除服务。
```c++
/**
 * 维护服务名和服务地址列表的多重映射
 * serviceName -> serviceAddress1
 *             -> serviceAddress2
 *             ...
 */
```
### 服务发现
虽然服务调用是服务消费方直接发向服务提供方的，但是分布式的服务，都是集群部署的，
服务的提供者数量也是动态变化的，
所以服务的地址也就无法预先确定。
因此如何发现这些服务就需要一个统一注册中心来承载。

客户端从注册中心获取服务，它需要在发起调用时，
从注册中心拉取开放服务的服务器地址列表存入本地缓存，

RPC连接池采用LRU淘汰算法，关闭最旧未使用的连接。

### 负载均衡
实现通用类型负载均衡路由引擎（工厂）。
通过路由引擎获取指定枚举类型的负载均衡器，
降低了代码耦合，规范了各个负载均衡器的使用，减少出错的可能。

提供了三种路由策略（随机、轮询、哈希）, 
由客户端使用，在客户端实现负载均衡
```c++

/**
 * @brief: 路由均衡引擎
 */
template<class T>
class RouteEngine {
public:
    static typename RouteStrategy<T>::ptr queryStrategy(typename RouteStrategy<T>::Strategy routeStrategyEnum) {
        switch (routeStrategyEnum){
            case RouteStrategy<T>::Random:
                return s_randomRouteStrategy;
            case RouteStrategy<T>::Polling:
                return std::make_shared<impl::PollingRouteStrategyImpl<T>>();
            case RouteStrategy<T>::HashIP:
                return s_hashIPRouteStrategy ;
            default:
                return s_randomRouteStrategy ;
        }
    }
private:
    static typename RouteStrategy<T>::ptr s_randomRouteStrategy;
    static typename RouteStrategy<T>::ptr s_hashIPRouteStrategy;
};
```
选择客户端负载均衡策略，根据路由策略选择服务地址，默认随机策略。
```c++
acid::rpc::RouteStrategy<std::string>::ptr strategy =
        acid::rpc::RouteEngine<std::string>::queryStrategy(
                acid::rpc::RouteStrategy<std::string>::Random);
```
### 心跳机制
服务中心必须管理服务器的存活状态，也就是健康检查。
注册服务的这一组机器，当这个服务组的某台机器如果出现宕机或者服务死掉的时候，
就会剔除掉这台机器。这样就实现了自动监控和管理。

项目采用了心跳发送的方式来检查健康状态。

服务器端：
开启一个计时器，定期给服务中心发心跳包，服务中心会回一个心跳包。

服务中心：
开启一个定时器，每次收到一个消息就更新一次定时器。如果倒计时结束，则判断服务掉线。


### 三种异步调用方式
整个框架最终都要落实在服务消费者。为了方便用户，满足用户的不同需求，项目设计了三种异步调用方式。

1. 以同步的方式异步调用

整个框架本身基于协程池，所以在遇到阻塞时会自动调度实现以同步的方式异步调用
```c++
auto sync_call = con->call<int>("add", 123, 321);
ACID_LOG_INFO(g_logger) << sync_call.getVal();
```
2. future 形式的异步调用

调用时会立即返回一个future
```c++
auto async_call_future = con->async_call<int>("add", 123, 321);
ACID_LOG_INFO(g_logger) << async_call_future.get().getVal();
```
3. 异步回调

收到消息时执行回调
```c++
con->async_call<int>([](acid::rpc::Result<int> res){
ACID_LOG_INFO(g_logger) << res.getVal();
}, "add", 123, 321); 
```

## 最后
通过以上介绍，我们粗略地了解了分布式服务的大概流程。但篇幅有限，无法面面俱到，
更多细节就需要去阅读代码来理解了。

这并不是终点，项目只是实现了简单的服务注册、发现，
数据同步，强一致性，选举机制等都没有实现，欢迎有能力者为项目添砖加瓦。

## 示例

rpc服务注册中心
```c++
#include "acid/rpc/rpc_service_registry.h"

// 服务注册中心
void Main() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8080");
    acid::rpc::RpcServiceRegistry::ptr server = std::make_shared<acid::rpc::RpcServiceRegistry>();
    // 服务注册中心绑定在8080端口
    while (!server->bind(address)){
        sleep(1);
    }
    server->start();
}

int main() {
    acid::IOManager::ptr loop = std::make_shared<acid::IOManager>();
    loop->submit(Main);
}
```
rpc 服务提供者
```c++
#include "acid/rpc/rpc_server.h"

int add(int a,int b){
    return a + b;
}

std::string getStr() {
    return  "hello world";
}

// 向服务中心注册服务，并处理客户端请求
void Main() {
    int port = 9000;
    acid::Address::ptr local = acid::IPv4Address::Create("127.0.0.1",port);
    acid::Address::ptr registry = acid::Address::LookupAny("127.0.0.1:8080");

    acid::rpc::RpcServer::ptr server = std::make_shared<acid::rpc::RpcServer>();

    // 注册服务，支持函数指针和函数对象
    server->registerMethod("add",add);
    server->registerMethod("getStr",getStr);
    server->registerMethod("echo", [](std::string str){
        return str;
    });

    // 先绑定本地地址
    while (!server->bind(local)){
        sleep(1);
    }
    // 绑定服务注册中心
    server->bindRegistry(registry);
    // 开始监听并处理服务请求
    server->start();
}

int main() {
    acid::IOManager::ptr loop = std::make_shared<acid::IOManager>();
    loop->submit(Main);
}
```
rpc 服务消费者，并不直接用RpcClient，而是采用更高级的封装，RpcConnectionPool。
提供了连接池和服务地址缓存。
```c++
#include "acid/log.h"
#include "acid/rpc/rpc_connection_pool.h"

static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

// 连接服务中心，自动服务发现，执行负载均衡决策，同时会缓存发现的结果
void Main() {
    acid::Address::ptr registry = acid::Address::LookupAny("127.0.0.1:8080");

    // 设置连接池的数量
    acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>(5);

    // 连接服务中心
    con->connect(registry);

    // 第一种调用接口，以同步的方式异步调用，原理是阻塞读时会在协程池里调度
    acid::rpc::Result<int> sync_call = con->call<int>("add", 123, 321);
    ACID_LOG_INFO(g_logger) << sync_call.getVal();

    // 第二种调用接口，future 形式的异步调用，调用时会立即返回一个future
    std::future<acid::rpc::Result<int>> async_call_future = con->async_call<int>("add", 123, 321);
    ACID_LOG_INFO(g_logger) << async_call_future.get().getVal();

    // 第三种调用接口，异步回调
    con->async_call<int>([](acid::rpc::Result<int> res){
        ACID_LOG_INFO(g_logger) << res.getVal();
    }, "add", 123, 321);

}

int main() {
    acid::IOManager::ptr loop = std::make_shared<acid::IOManager>();
    loop->submit(Main);
}
```

## 反馈和参与

* bug、疑惑、修改建议都欢迎提在[Github Issues](https://github.com/zavier-wong/acid/issues)中
* 也可发送邮件 [acid@163.com](mailto:acid@163.com) 交流学习

## 致谢

感谢 [sylar](https://github.com/sylar-yin/sylar) 项目, 跟随着sylar完成了网络部分，才下定决心写一个RPC框架。在RPC框架的设计与实现上也深度参考了sylar项目。